import React, { useState, useRef, useEffect } from 'react';
import { User, Message, Room } from '../types';
import { Send, Image as ImageIcon, LogOut, Sparkles, Loader2, Hash, Plus, X, Users, MessageSquare, AlertCircle, RefreshCw, Search } from 'lucide-react';
import { GoogleGenAI } from '@google/genai';
import * as flatbuffers from 'flatbuffers';
import localforage from 'localforage';
import { RootMessage, AnyPayload, ServerMessagePayload, RequestMessagePayload, MessageAckPayload, MsgContentType, SignalingFromServerPayload, PongPayload, BatchPullRoomHistoryPayload, ServerMessageItem, ChatType } from '../generated/chat_app';
import { encodeClientMessage, encodePullMissingMessages, encodeRequestRoomHistory, encodeBatchPullMessage, encodeSignalingFromClient, encodeSignalingFromClientJoin, encodePing } from '../utils/fb-helper';
import { JoinSessionPayload } from '../generated/chat_app';

// 智能解析时间戳（兼容秒级和毫秒级）
const parseTimestamp = (ts: any) => {
  if (!ts) return new Date();
  const numTs = Number(ts);
  // 如果时间戳数字小于 100000000000 (12位)，说明是秒级时间戳，需要乘 1000
  if (numTs < 100000000000) {
    return new Date(numTs * 1000);
  }
  // 否则说明已经是毫秒级时间戳，直接使用
  return new Date(numTs);
};

interface ChatProps {
  user: User;
  onLogout: () => void;
}

export function Chat({ user, onLogout }: ChatProps) {
  const [currentUser, setCurrentUser] = useState<User>(user);
  const [rooms, setRooms] = useState<Room[]>([]);
  const [currentRoom, setCurrentRoom] = useState<Room | null>(null);
  const [messages, setMessages] = useState<Record<string, Message[]>>({});
  const [hasMoreMessages, setHasMoreMessages] = useState<Record<string, boolean>>({});
  const [inputValue, setInputValue] = useState('');
  const [isGenerating, setIsGenerating] = useState(false);
  const [ws, setWs] = useState<WebSocket | null>(null);
  
  // 创建房间相关的状态
  const [showCreateRoom, setShowCreateRoom] = useState(false);
  const [newRoomName, setNewRoomName] = useState('');
  const [newRoomDesc, setNewRoomDesc] = useState('');
  const [isAiReplying, setIsAiReplying] = useState(false);
  const [isLoadingHistory, setIsLoadingHistory] = useState(false);
  const [unreadCount, setUnreadCount] = useState(0);
  const [isLocalLoaded, setIsLocalLoaded] = useState(false);
  const [joinRoomQuery, setJoinRoomQuery] = useState('');
  const [isJoiningRoom, setIsJoiningRoom] = useState(false);
  const [joinRoomError, setJoinRoomError] = useState('');
  const [isCreatingRoom, setIsCreatingRoom] = useState(false);
  const [createRoomError, setCreateRoomError] = useState('');
  const [replyingTo, setReplyingTo] = useState<Message | null>(null);
  const [selectedUser, setSelectedUser] = useState<{id: string, name: string} | null>(null);

  // 网络状态相关状态
  const [connectionState, setConnectionState] = useState<'connected' | 'connecting' | 'disconnected'>('connecting');
  const [rtt, setRtt] = useState<number | null>(null);
  const [reconnectAttempt, setReconnectAttempt] = useState(0);
  
  const pendingCreateRoomNameRef = useRef<string | null>(null);
  const pendingCreateRoomDescRef = useRef<string | null>(null);

  const messagesEndRef = useRef<HTMLDivElement>(null);
  const messageContainerRef = useRef<HTMLDivElement>(null);
  const isAtBottomRef = useRef<boolean>(true);
  const currentRoomIdRef = useRef<string | null>(null);
  const roomScrollPositionsRef = useRef<Record<string, number>>({});
  const networkRetryTimersRef = useRef<Record<string, NodeJS.Timeout>>({});
  const businessTimeoutTimersRef = useRef<Record<string, NodeJS.Timeout>>({});
  const roomMaxServerMsgIdRef = useRef<Record<string, number>>({});
  const pendingMessagesRef = useRef<Record<string, Record<number, any>>>({});
  const roomsRef = useRef<Room[]>([]);
  
  const pingIntervalRef = useRef<NodeJS.Timeout | null>(null);
  const pingTimeoutRef = useRef<NodeJS.Timeout | null>(null);
  const reconnectTimerRef = useRef<NodeJS.Timeout | null>(null);
  const reconnectAttemptRef = useRef<number>(0);
  const isConnectingRef = useRef<boolean>(false);
  const wsRef = useRef<WebSocket | null>(null);

  useEffect(() => {
    roomsRef.current = rooms;
  }, [rooms]);

  useEffect(() => {
    let isMounted = true;
    
    localforage.config({
      name: 'XChat',
      storeName: 'messages'
    });

    const loadLocalData = async () => {
      try {
        let savedRooms = await localforage.getItem<Room[]>(`rooms_${user.username}`) || [];
        
        // Remove legacy default rooms
        const legacyIds = ['general', 'tech', 'random'];
        savedRooms = savedRooms.filter(r => !legacyIds.includes(r.id));

        const savedMessages = await localforage.getItem<Record<string, Message[]>>(`messages_${user.username}`) || {};
        
        if (!isMounted) return;
        
        for (const [roomId, msgs] of Object.entries(savedMessages)) {
          let maxId = 0;
          msgs.forEach((m) => {
            if (m.serverMessageId !== undefined && m.serverMessageId > maxId) {
              maxId = m.serverMessageId;
            }
          });
          roomMaxServerMsgIdRef.current[roomId] = maxId;
        }

        setRooms(savedRooms);
        setMessages(savedMessages);
        
        if (savedRooms.length > 0 && !currentRoomIdRef.current) {
          setCurrentRoom(savedRooms[0]);
        }
        
        setIsLocalLoaded(true);
      } catch (err) {
        console.error("Failed to load local data", err);
        if (isMounted) setIsLocalLoaded(true);
      }
    };
    
    loadLocalData();
    
    return () => {
      isMounted = false;
    };
  }, [user.username]);

  useEffect(() => {
    currentRoomIdRef.current = currentRoom?.id || null;
    setUnreadCount(0);
    
    if (currentRoom) {
      // 使用 setTimeout 确保 React 已经渲染了新房间的消息列表
      setTimeout(() => {
        if (!messageContainerRef.current) return;
        
        const savedScrollTop = roomScrollPositionsRef.current[currentRoom.id];
        if (savedScrollTop !== undefined) {
          // 恢复之前保存的滚动位置
          messageContainerRef.current.scrollTop = savedScrollTop;
          const { scrollHeight, clientHeight } = messageContainerRef.current;
          isAtBottomRef.current = scrollHeight - savedScrollTop - clientHeight < 50;
        } else {
          // 如果没有保存过位置，默认滚动到底部
          isAtBottomRef.current = true;
          scrollToBottom();
        }
      }, 10);
    }
  }, [currentRoom]);

  const scrollToBottom = () => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  };

  const connectWebSocket = () => {
    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) return;
    if (isConnectingRef.current) return;
    isConnectingRef.current = true;

    setConnectionState(reconnectAttemptRef.current < 2 && reconnectAttemptRef.current > 0 ? 'connecting' : (reconnectAttemptRef.current >= 2 ? 'disconnected' : 'connecting'));

    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws`;
    const socket = new WebSocket(wsUrl);

    socket.binaryType = 'arraybuffer';

    socket.onopen = () => {
      console.log('WebSocket connected');
      isConnectingRef.current = false;
      setConnectionState('connected');
      setReconnectAttempt(0);
      reconnectAttemptRef.current = 0;
      
      // Clear timers
      if (reconnectTimerRef.current) clearTimeout(reconnectTimerRef.current);
      if (pingIntervalRef.current) clearInterval(pingIntervalRef.current);
      if (pingTimeoutRef.current) clearTimeout(pingTimeoutRef.current);

      setWs(socket);
      wsRef.current = socket;

      // Start ping interval
      pingIntervalRef.current = setInterval(() => {
        if (socket.readyState === WebSocket.OPEN) {
          const ts = Date.now();
          const buf = encodePing(ts);
          socket.send(buf);

          // Expect pong within 3s
          pingTimeoutRef.current = setTimeout(() => {
            console.log('Ping timeout, closing socket...');
            if (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING) {
              socket.close();
            }
          }, 3000);
        }
      }, 30000);

      // 发起批量拉取消息
      const roomsToPull = roomsRef.current.length > 0 ? roomsRef.current : [];
      if (roomsToPull.length > 0) {
        const pullReqs = roomsToPull.map(r => ({
          roomId: r.id,
          maxId: roomMaxServerMsgIdRef.current[r.id] || 0
        }));
        const buf = encodeBatchPullMessage(pullReqs);
        socket.send(buf);
      }
    };

    socket.onmessage = async (event) => {
      try {
        if (event.data instanceof ArrayBuffer || event.data instanceof Blob) {
          const arrayBuffer = event.data instanceof Blob ? await event.data.arrayBuffer() : event.data;
          const buf = new flatbuffers.ByteBuffer(new Uint8Array(arrayBuffer));
          const root = RootMessage.getRootAsRootMessage(buf);
          const payloadType = root.payloadType();

          if (payloadType === AnyPayload.PongPayload) {
            const pongPayload = root.payload(new PongPayload()) as PongPayload;
            const originTs = Number(pongPayload.ts());
            const currentRtt = Date.now() - originTs;
            setRtt(currentRtt);
            if (pingTimeoutRef.current) {
              clearTimeout(pingTimeoutRef.current);
              pingTimeoutRef.current = null;
            }
            return;
          }

          if (payloadType === AnyPayload.BatchPullRoomHistoryPayload) {
            const payload = root.payload(new BatchPullRoomHistoryPayload()) as BatchPullRoomHistoryPayload;
            const roomId = payload.roomId() || 'general';
            const chunksLen = payload.historyChunksLength();

            const newMsgs: Message[] = [];
            for (let i = 0; i < chunksLen; i++) {
              const chunk = payload.historyChunks(i);
              const dataArray = chunk?.dataArray();
              if (dataArray) {
                try {
                  const chunkBuf = new flatbuffers.ByteBuffer(dataArray);
                  const pos = chunkBuf.readInt32(chunkBuf.position()) + chunkBuf.position();
                  
                  const itemCheck = new ServerMessageItem();
                  itemCheck.__init(pos, chunkBuf);
                  
                  let parsedMsgs: Message[] = [];
                  
                  if (itemCheck.timestamp() > 0n || itemCheck.user() !== null) {
                      parsedMsgs.push({
                        id: itemCheck.serverMessageId()?.toString() || Date.now().toString(),
                        clientMessageId: itemCheck.clientMessageId() || undefined,
                        serverMessageId: Number(itemCheck.serverMessageId()),
                        sender: itemCheck.user()?.username() || 'Unknown',
                        senderId: Number(itemCheck.user()?.userid() || 0),
                        content: itemCheck.content() || '',
                        timestamp: parseTimestamp(Number(itemCheck.timestamp())),
                        type: itemCheck.msgType() === MsgContentType.Image ? 'image' : 'text',
                        imageUrl: itemCheck.imageUrl() || undefined,
                        replyTo: Number(itemCheck.replyTo()) || undefined,
                        status: 'success'
                      });
                  } else {
                      let serverPayload: ServerMessagePayload | null = null;
                      try {
                        const rootMsg = RootMessage.getRootAsRootMessage(chunkBuf);
                        if (rootMsg.payloadType() === AnyPayload.ServerMessagePayload) {
                           serverPayload = rootMsg.payload(new ServerMessagePayload()) as ServerMessagePayload;
                        }
                      } catch (e) {}
  
                      if (!serverPayload) {
                        serverPayload = new ServerMessagePayload();
                        serverPayload.__init(pos, chunkBuf);
                      }
  
                      if (serverPayload) {
                         try {
                           const item = serverPayload.messages();
                           if (item) {
                              const senderInfo = item.user()?.username();
                              const contentInfo = item.content();
                              
                              // Make sure it is not a garbage parsed item
                              if (senderInfo || contentInfo !== null || item.timestamp() > 0n) {
                                parsedMsgs.push({
                                  id: item.serverMessageId()?.toString() || Date.now().toString(),
                                  clientMessageId: item.clientMessageId() || undefined,
                                  serverMessageId: Number(item.serverMessageId()),
                                  sender: senderInfo || 'Unknown',
                                  senderId: Number(item.user()?.userid() || 0),
                                  content: contentInfo || '',
                                  timestamp: parseTimestamp(Number(item.timestamp())),
                                  type: item.msgType() === MsgContentType.Image ? 'image' : 'text',
                                  imageUrl: item.imageUrl() || undefined,
                                  replyTo: Number(item.replyTo()) || undefined,
                                  status: 'success'
                                });
                              }
                           }
                         } catch (e) {
                           console.error("Failed to parse inner ServerMessagePayload", e);
                         }
                      }
                  }

                  newMsgs.push(...parsedMsgs.filter(m => m.serverMessageId > 0 || m.content !== ''));
                } catch (e) {
                  console.error('Failed to parse chunk from BatchPullRoomHistoryPayload:', e);
                }
              }
            }

            if (newMsgs.length > 0) {
              setMessages(prev => {
                const existingMsgs = prev[roomId] || [];
                const existingIds = new Set(existingMsgs.map(m => m.id));
                const filtered = newMsgs.filter(m => !existingIds.has(m.id));
                if (filtered.length === 0) return prev;
                
                const merged = [...existingMsgs, ...filtered].sort((a,b) => a.timestamp.getTime() - b.timestamp.getTime());
                
                let maxId = roomMaxServerMsgIdRef.current[roomId] || 0;
                merged.forEach(m => {
                  if (m.serverMessageId !== undefined && m.serverMessageId > maxId) {
                    maxId = m.serverMessageId;
                  }
                });
                roomMaxServerMsgIdRef.current[roomId] = maxId;

                return { ...prev, [roomId]: merged };
              });
            }
            return;
          }

          if (payloadType === AnyPayload.ServerMessagePayload) {
            const payload = root.payload(new ServerMessagePayload()) as ServerMessagePayload;
            const roomId = payload.sessionId() || 'general';
            
            let messagesToAdd: Message[] = [];
            let hasNewMessages = false;

            const processMsg = (msgRaw: any, sMsgId: number) => {
              const parsedMsg: Message = {
                id: msgRaw.id || Date.now().toString(),
                clientMessageId: msgRaw.clientMessageId,
                serverMessageId: sMsgId,
                sender: msgRaw.user?.username || 'Unknown',
                senderId: msgRaw.user?.userid || undefined,
                content: msgRaw.content,
                timestamp: parseTimestamp(msgRaw.timestamp),
                type: msgRaw.msgType || 'text',
                imageUrl: msgRaw.imageUrl,
                replyTo: msgRaw.replyTo
              };
              messagesToAdd.push(parsedMsg);
              hasNewMessages = true;

              if (!isNaN(sMsgId)) {
                roomMaxServerMsgIdRef.current[roomId] = sMsgId;
                const nextId = sMsgId + 1;
                if (pendingMessagesRef.current[roomId] && pendingMessagesRef.current[roomId][nextId]) {
                  const nextMsgRaw = pendingMessagesRef.current[roomId][nextId];
                  delete pendingMessagesRef.current[roomId][nextId];
                  processMsg(nextMsgRaw, nextId);
                }
              }
            };

            const incomingMessages = [];
            const msg = payload.messages();
            if (msg) {
              incomingMessages.push({
                id: msg.serverMessageId().toString(),
                clientMessageId: msg.clientMessageId(),
                serverMessageId: Number(msg.serverMessageId()),
                content: msg.content(),
                user: { username: msg.user()?.username() || 'Unknown', userid: Number(msg.user()?.userid() || 0) },
                timestamp: Number(msg.timestamp()),
                msgType: msg.msgType() === MsgContentType.Image ? 'image' : 'text',
                imageUrl: msg.imageUrl() || undefined,
                replyTo: Number(msg.replyTo()) || undefined
              });
            }

            // 按 serverMessageId 排序，以防后端批量下发时乱序
            const sortedMessages = incomingMessages.sort((a, b) => {
              const idA = a.serverMessageId !== undefined ? Number(a.serverMessageId) : 0;
              const idB = b.serverMessageId !== undefined ? Number(b.serverMessageId) : 0;
              return idA - idB;
            });

            sortedMessages.forEach((msgRaw: any) => {
              const serverMessageId = msgRaw.serverMessageId !== undefined ? Number(msgRaw.serverMessageId) : NaN;

              if (!isNaN(serverMessageId)) {
                const currentMax = roomMaxServerMsgIdRef.current[roomId];
                
                if (currentMax === undefined) {
                  processMsg(msgRaw, serverMessageId);
                } else if (serverMessageId <= currentMax) {
                  console.log(`Ignored duplicate or old message ${serverMessageId} for room ${roomId}`);
                } else if (serverMessageId === currentMax + 1) {
                  processMsg(msgRaw, serverMessageId);
                } else {
                  if (!pendingMessagesRef.current[roomId]) {
                    pendingMessagesRef.current[roomId] = {};
                  }
                  pendingMessagesRef.current[roomId][serverMessageId] = msgRaw;
                  
                  const missingIds: number[] = [];
                  for (let id = currentMax + 1; id < serverMessageId; id++) {
                    if (!pendingMessagesRef.current[roomId][id]) {
                      missingIds.push(id);
                    }
                  }
                  
                  if (missingIds.length > 0) {
                    const delay = Math.floor(Math.random() * 1900) + 100;
                    setTimeout(() => {
                      const currentMaxNow = roomMaxServerMsgIdRef.current[roomId] || 0;
                      const stillMissing = missingIds.filter(id => 
                        currentMaxNow < id && !pendingMessagesRef.current[roomId]?.[id]
                      );
                      
                      if (stillMissing.length > 0 && socket.readyState === WebSocket.OPEN) {
                        const pullReqBuf = encodePullMissingMessages(roomId, stillMissing);
                        socket.send(pullReqBuf);
                      }
                    }, delay);
                  }
                }
              } else {
                processMsg(msgRaw, NaN);
              }
            });

            if (hasNewMessages) {
              const roomExists = roomsRef.current.some(r => r.id === roomId);
              if (!roomExists) {
                const isSingleChat = payload.chatType() === ChatType.Single;
                const newRoom: Room = {
                  id: roomId,
                  name: isSingleChat ? (messagesToAdd[0]?.sender || roomId) : roomId,
                  chatType: payload.chatType(),
                  description: isSingleChat ? 'Direct Message' : 'Group Chat'
                };
                setRooms(prev => [newRoom, ...prev]);
              }

              setMessages(prev => {
                const existingRoomMsgs = prev[roomId] || [];
                const newRoomMsgs = [...existingRoomMsgs];
                
                messagesToAdd.forEach(newMsg => {
                  if (newMsg.clientMessageId) {
                    if (networkRetryTimersRef.current[newMsg.clientMessageId]) {
                      clearInterval(networkRetryTimersRef.current[newMsg.clientMessageId]);
                      delete networkRetryTimersRef.current[newMsg.clientMessageId];
                    }
                    if (businessTimeoutTimersRef.current[newMsg.clientMessageId]) {
                      clearTimeout(businessTimeoutTimersRef.current[newMsg.clientMessageId]);
                      delete businessTimeoutTimersRef.current[newMsg.clientMessageId];
                    }
                  }

                  const existingIdx = newRoomMsgs.findIndex(m => 
                    (m.clientMessageId && m.clientMessageId === newMsg.clientMessageId) || 
                    m.id === newMsg.id
                  );

                  if (existingIdx >= 0) {
                    newRoomMsgs[existingIdx] = {
                      ...newRoomMsgs[existingIdx],
                      ...newMsg,
                      status: 'success'
                    };
                  } else {
                    newRoomMsgs.push({ ...newMsg, status: 'success' });
                  }
                });

                newRoomMsgs.sort((a, b) => a.timestamp.getTime() - b.timestamp.getTime());
                return {
                  ...prev,
                  [roomId]: newRoomMsgs
                };
              });
              
              if (roomId === currentRoomIdRef.current) {
                if (isAtBottomRef.current) {
                  setTimeout(scrollToBottom, 50);
                } else {
                  setUnreadCount(prev => prev + messagesToAdd.length);
                }
              }
            }
          } else if (payloadType === AnyPayload.RequestMessagePayload) {
            const payload = root.payload(new RequestMessagePayload()) as RequestMessagePayload;
            const roomId = payload.roomId() || 'general';
            const hasMore = payload.hasMoreMessages();
            const messagesLen = payload.messagesLength();
            
            const incomingMessages = [];
            for (let i = 0; i < messagesLen; i++) {
              const msg = payload.messages(i);
              if (msg) {
                incomingMessages.push({
                  id: msg.id() || Date.now().toString(),
                  clientMessageId: undefined, // RequestMessageItem schema doesn't have client_message_id
                  sender: msg.user()?.username() || 'Unknown',
                  senderId: Number(msg.user()?.userid() || 0),
                  content: msg.content(),
                  timestamp: parseTimestamp(Number(msg.timestamp())),
                  type: msg.msgType() === MsgContentType.Image ? 'image' : 'text',
                  imageUrl: msg.imageUrl() || undefined
                });
              }
            }

            setHasMoreMessages(prev => ({
              ...prev,
              [roomId]: hasMore
            }));

            const container = messageContainerRef.current;
            const previousScrollHeight = container ? container.scrollHeight : 0;
            const previousScrollTop = container ? container.scrollTop : 0;

            setMessages(prev => {
              const existingMessages = prev[roomId] || [];
              const updatedRoomMessages = [...incomingMessages, ...existingMessages];
              updatedRoomMessages.sort((a, b) => a.timestamp.getTime() - b.timestamp.getTime());
              return {
                ...prev,
                [roomId]: updatedRoomMessages
              };
            });
            
            setTimeout(() => {
              if (container) {
                const newScrollHeight = container.scrollHeight;
                container.scrollTop = previousScrollTop + (newScrollHeight - previousScrollHeight);
              }
              setIsLoadingHistory(false);
            }, 0);
          } else if (payloadType === AnyPayload.MessageAckPayload) {
            const payload = root.payload(new MessageAckPayload()) as MessageAckPayload;
            const clientMessageId = payload.clientMessageId();
            const status = payload.status();
            
            if (clientMessageId) {
              if (status === 'SUCCESS') {
                if (networkRetryTimersRef.current[clientMessageId]) {
                  clearInterval(networkRetryTimersRef.current[clientMessageId]);
                  delete networkRetryTimersRef.current[clientMessageId];
                }
                if (businessTimeoutTimersRef.current[clientMessageId]) {
                  clearTimeout(businessTimeoutTimersRef.current[clientMessageId]);
                  delete businessTimeoutTimersRef.current[clientMessageId];
                }
                setMessages(prev => {
                  const newMessages = { ...prev };
                  for (const rId in newMessages) {
                    newMessages[rId] = newMessages[rId].map(m => 
                      m.clientMessageId === clientMessageId ? { ...m, status: 'success' } : m
                    );
                  }
                  return newMessages;
                });
              } else {
                if (networkRetryTimersRef.current[clientMessageId]) {
                  clearInterval(networkRetryTimersRef.current[clientMessageId]);
                  delete networkRetryTimersRef.current[clientMessageId];
                }
                if (businessTimeoutTimersRef.current[clientMessageId]) {
                  clearTimeout(businessTimeoutTimersRef.current[clientMessageId]);
                  delete businessTimeoutTimersRef.current[clientMessageId];
                }
                setMessages(prev => {
                  const newMessages = { ...prev };
                  for (const rId in newMessages) {
                    newMessages[rId] = newMessages[rId].map(m => 
                      m.clientMessageId === clientMessageId ? { ...m, status: 'failed' } : m
                    );
                  }
                  return newMessages;
                });
              }
            }
          } else if (payloadType === AnyPayload.SignalingFromServerPayload) {
            const payload = root.payload(new SignalingFromServerPayload()) as SignalingFromServerPayload;
            const action = payload.action();
            const roomId = payload.roomId();
            const status = payload.status();

            if (action === 'subscribe_ack' && status === 'ok' && roomId) {
              const newRoom = {
                id: roomId,
                name: pendingCreateRoomNameRef.current || 'New Room',
                description: pendingCreateRoomDescRef.current || ''
              };

              setRooms(prev => {
                if (prev.some(r => r.id === newRoom.id)) return prev;
                return [...prev, newRoom];
              });
              
              setCurrentRoom(newRoom);
              setNewRoomName('');
              setNewRoomDesc('');
              setShowCreateRoom(false);
              setIsCreatingRoom(false);
              setCreateRoomError('');
            }
          } else if (payloadType === AnyPayload.JoinSessionPayload) {
            const joinPayload = root.payload(new JoinSessionPayload()) as JoinSessionPayload;
            const roomId = joinPayload.roomId() || '';
            const roomName = joinPayload.roomname() || '';
            const messagesLen = joinPayload.messagesLength();

            const newMsgs: Message[] = [];
            for (let i = 0; i < messagesLen; i++) {
               const msg = joinPayload.messages(i);
               if (msg) {
                  newMsgs.push({
                    id: msg.serverMessageId().toString(), // Using serverMessageId as fallback id
                    clientMessageId: msg.clientMessageId() || undefined,
                    serverMessageId: Number(msg.serverMessageId()),
                    sender: msg.user()?.username() || 'Unknown',
                    senderId: Number(msg.user()?.userid() || 0),
                    content: msg.content() || '',
                    timestamp: parseTimestamp(Number(msg.timestamp())),
                    type: msg.msgType() === MsgContentType.Image ? 'image' : 'text',
                    imageUrl: msg.imageUrl() || undefined,
                    replyTo: Number(msg.replyTo()) || undefined
                  });
               }
            }

            const newRoom: Room = {
              id: roomId,
              name: roomName,
              description: ''
            };

            setRooms(prev => {
              if (!prev.find(r => r.id === newRoom.id)) {
                return [...prev, newRoom];
              }
              return prev;
            });

            setMessages(prev => {
              const existingMsgs = prev[roomId] || [];
              const existingIds = new Set(existingMsgs.map(m => m.id));
              const filtered = newMsgs.filter(m => !existingIds.has(m.id));
              const merged = [...existingMsgs, ...filtered].sort((a,b) => a.timestamp.getTime() - b.timestamp.getTime());
              
              let maxId = roomMaxServerMsgIdRef.current[roomId] || 0;
              merged.forEach(m => {
                if (m.serverMessageId !== undefined && m.serverMessageId > maxId) {
                  maxId = m.serverMessageId;
                }
              });
              roomMaxServerMsgIdRef.current[roomId] = maxId;

              return { ...prev, [roomId]: merged };
            });

            setJoinRoomQuery('');
            setCurrentRoom(newRoom);
            setIsJoiningRoom(false);
            setJoinRoomError('');
          }
          return;
        }

      } catch (e) {
        console.error('Failed to parse message', e);
      }
    };

    socket.onclose = () => {
      console.log('WebSocket disconnected');
      isConnectingRef.current = false;
      wsRef.current = null;
      setWs(null);
      handleReconnect();
    };
  };

  const handleReconnect = () => {
    // Clear ping timers
    if (pingIntervalRef.current) clearInterval(pingIntervalRef.current);
    if (pingTimeoutRef.current) clearTimeout(pingTimeoutRef.current);

    const attempt = reconnectAttemptRef.current;
    
    // UI state
    if (attempt < 2) {
      setConnectionState('connecting');
    } else {
      setConnectionState('disconnected');
    }

    // Exponential backoff
    const baseDelay = attempt === 0 ? 0 : Math.min(60000, Math.pow(2, attempt - 1) * 2000);
    // Add jitter
    const actualDelay = baseDelay * (0.8 + Math.random() * 0.4);

    reconnectAttemptRef.current += 1;
    setReconnectAttempt(reconnectAttemptRef.current);

    console.log(`Reconnecting (attempt ${attempt + 1}) in ${Math.round(actualDelay)}ms...`);
    
    if (reconnectTimerRef.current) clearTimeout(reconnectTimerRef.current);
    reconnectTimerRef.current = setTimeout(() => {
      connectWebSocket();
    }, actualDelay);
  };

  useEffect(() => {
    if (!isLocalLoaded) return;
    connectWebSocket();
    return () => {
      if (wsRef.current) wsRef.current.close();
      if (reconnectTimerRef.current) clearTimeout(reconnectTimerRef.current);
      if (pingIntervalRef.current) clearInterval(pingIntervalRef.current);
      if (pingTimeoutRef.current) clearTimeout(pingTimeoutRef.current);
    };
  }, [user.username, isLocalLoaded]);

  // Persist messages to IndexedDB
  useEffect(() => {
    if (isLocalLoaded && Object.keys(messages).length > 0) {
      localforage.setItem(`messages_${user.username}`, messages).catch(console.error);
    }
  }, [messages, user.username, isLocalLoaded]);

  // Persist rooms to IndexedDB
  useEffect(() => {
    if (isLocalLoaded && rooms.length > 0) {
      localforage.setItem(`rooms_${user.username}`, rooms).catch(console.error);
    }
  }, [rooms, user.username, isLocalLoaded]);

  const sendClientMessage = (roomId: string, content: string, clientMessageId: string, isRetry = false, msgType: 'text' | 'image' = 'text', imageUrl?: string, replyTo?: number) => {
    const sendToWs = () => {
      if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
        const targetRoom = roomsRef.current.find(r => r.id === roomId);
        const chatType = targetRoom?.chatType === ChatType.Single ? ChatType.Single : ChatType.Group;
        const buf = encodeClientMessage(chatType, roomId, clientMessageId, content, msgType, imageUrl, replyTo);
        wsRef.current.send(buf);
      }
    };

    // 首次发送
    sendToWs();

    // 1. 网络重传定时器 (2s)
    if (networkRetryTimersRef.current[clientMessageId]) {
      clearInterval(networkRetryTimersRef.current[clientMessageId]);
    }
    networkRetryTimersRef.current[clientMessageId] = setInterval(() => {
      console.log(`[Network Retry] Resending message ${clientMessageId}...`);
      sendToWs();
    }, 2000);

    // 2. 业务超时定时器 (10s)
    if (businessTimeoutTimersRef.current[clientMessageId]) {
      clearTimeout(businessTimeoutTimersRef.current[clientMessageId]);
    }
    businessTimeoutTimersRef.current[clientMessageId] = setTimeout(() => {
      console.log(`[Business Timeout] Message ${clientMessageId} failed.`);
      // 清理网络重传定时器
      if (networkRetryTimersRef.current[clientMessageId]) {
        clearInterval(networkRetryTimersRef.current[clientMessageId]);
        delete networkRetryTimersRef.current[clientMessageId];
      }
      delete businessTimeoutTimersRef.current[clientMessageId];
      
      // 更新 UI 状态为 failed
      setMessages(prev => {
        const roomMsgs = prev[roomId] || [];
        return {
          ...prev,
          [roomId]: roomMsgs.map(m => 
            m.clientMessageId === clientMessageId ? { ...m, status: 'failed' } : m
          )
        };
      });
    }, 10000);

    if (!isRetry) {
      // 乐观更新 UI
      const msgForState: Message = {
        id: clientMessageId,
        clientMessageId,
        sender: currentUser.username,
        senderId: Number(currentUser.userid),
        content,
        timestamp: new Date(),
        type: 'text',
        status: 'sending'
      };
      setMessages(prev => ({
        ...prev,
        [roomId]: [...(prev[roomId] || []), msgForState]
      }));
      setTimeout(scrollToBottom, 50);
    } else {
      // 如果是重试，把状态改回 sending
      setMessages(prev => {
        const roomMsgs = prev[roomId] || [];
        return {
          ...prev,
          [roomId]: roomMsgs.map(m => 
            m.clientMessageId === clientMessageId ? { ...m, status: 'sending' } : m
          )
        };
      });
    }
  };

  const handleJoinRoom = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!joinRoomQuery.trim() || !currentUser.userid) return;

    setIsJoiningRoom(true);
    setJoinRoomError('');

    try {
      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), 10000);

      const response = await fetch('/api/joinsession', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ userid: currentUser.userid, roomname: joinRoomQuery.trim() }),
        signal: controller.signal
      });

      clearTimeout(timeoutId);

      if (!response.ok) {
        throw new Error('Failed to join room via HTTP');
      }

      const data = await response.json();
      if (data.errormsg) {
        setJoinRoomError(data.errormsg);
        setIsJoiningRoom(false);
        return;
      }

      const { roomid } = data;
      if (!roomid) {
        setJoinRoomError('服务器未返回 roomid');
        setIsJoiningRoom(false);
        return;
      }

      // Send SignalingFromClientJoinPayload using FlatBuffers over WebSocket
      if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
        // encodeSignalingFromClientJoin is imported from fb-helper
        const buf = encodeSignalingFromClientJoin("join_room", roomid.toString(), joinRoomQuery.trim());
        wsRef.current.send(buf);

        // 设置 WebSocket 确认超时
        setTimeout(() => {
          setIsJoiningRoom(prev => {
            if (prev) {
              setJoinRoomError('WebSocket 发送成功，但等待后端 JoinSession 返回数据超时(10秒)');
              return false;
            }
            return prev;
          });
        }, 10000);

      } else {
        setJoinRoomError('WebSocket 未连接，无法发送订阅信令');
        setIsJoiningRoom(false);
      }

    } catch (err: any) {
      if (err.name === 'AbortError') {
        setJoinRoomError('HTTP 接口 /api/joinsession 响应超时');
      } else {
        setJoinRoomError(err.message || '加入房间发生未知错误');
      }
      setIsJoiningRoom(false);
    }
  };

  const handleUserClick = (userinfo: {id: string | number, name: string}) => {
    setSelectedUser({id: userinfo.id.toString(), name: userinfo.name});
  };

  const handleStartDirectMessage = (userinfo: {id: string, name: string}) => {
    const existingRoom = rooms.find(r => r.id === userinfo.id && r.chatType === ChatType.Single);
    if (existingRoom) {
      setCurrentRoom(existingRoom);
    } else {
      const newRoom: Room = {
        id: userinfo.id,
        name: userinfo.name,
        chatType: ChatType.Single,
        description: 'Direct Message'
      };
      setRooms(prev => [newRoom, ...prev]);
      setCurrentRoom(newRoom);
    }
    setSelectedUser(null);
  };

  const handleSendMessage = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!inputValue.trim() || !currentRoom) return;

    const messageContent = inputValue;
    setInputValue('');

    const clientMessageId = `msg_${Date.now()}_${Math.random().toString(36).substring(2, 9)}`;
    sendClientMessage(currentRoom.id, messageContent, clientMessageId, false, 'text', undefined, replyingTo?.serverMessageId);
    setReplyingTo(null);

    // 检查是否 @ 了 AI
    if (messageContent.includes('@ai') || messageContent.includes('@AI')) {
      handleAiResponse(messageContent, currentRoom.id);
    }
  };

  const handleAiResponse = async (userMessage: string, roomId: string) => {
    setIsAiReplying(true);
    const tempId = `ai_${Date.now()}`;
    
    // 添加一个 "AI 正在思考" 的临时消息
    const thinkingMsg: Message = {
      id: tempId,
      sender: 'AI Assistant',
      content: '正在思考中...',
      timestamp: new Date(),
      type: 'text',
      isGenerating: true
    };

    setMessages(prev => ({
      ...prev,
      [roomId]: [...(prev[roomId] || []), thinkingMsg]
    }));
    
    // 添加思考消息后滚动到底部
    if (roomId === currentRoom?.id) {
      setTimeout(scrollToBottom, 50);
    }

    try {
      // 在 Vite 中，环境变量需要通过 import.meta.env 访问
      // @ts-ignore
      const apiKey = import.meta.env.VITE_GEMINI_API_KEY || import.meta.env.VITE_API_KEY;
      
      if (!apiKey) {
        throw new Error('未找到 Gemini API Key。请在 .env 文件中设置 VITE_GEMINI_API_KEY。');
      }

      const ai = new GoogleGenAI({ apiKey });
      
      // 提取 @ai 后面的真实问题
      const prompt = userMessage.replace(/@ai/gi, '').trim() || '你好';
      
      const response = await ai.models.generateContent({
        model: 'gemini-3-flash-preview',
        contents: prompt,
        config: {
          systemInstruction: "你是一个在聊天室里的 AI 助手。你的回答应该简短、幽默、有帮助。如果用户问你问题，请直接回答。不要使用 Markdown 格式，只输出纯文本。"
        }
      });

      const aiReply = response.text || '抱歉，我没听懂。';

      // 更新本地 UI，将思考中的消息替换为真实回复
      setMessages(prev => ({
        ...prev,
        [roomId]: (prev[roomId] || []).map(msg => 
          msg.id === tempId 
            ? { ...msg, content: aiReply, isGenerating: false }
            : msg
        )
      }));

      // 将 AI 的回复通过 WebSocket 广播给房间里的其他人
      const aiClientMessageId = `msg_${Date.now()}_${Math.random().toString(36).substring(2, 9)}`;
      if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
        const targetRoom = roomsRef.current.find(r => r.id === roomId);
        const chatType = targetRoom?.chatType === ChatType.Single ? ChatType.Single : ChatType.Group;
        const buf = encodeClientMessage(chatType, roomId, aiClientMessageId, aiReply, 'text');
        wsRef.current.send(buf);
      }

    } catch (error) {
      console.error('AI response failed:', error);
      setMessages(prev => ({
        ...prev,
        [roomId]: (prev[roomId] || []).map(msg => 
          msg.id === tempId 
            ? { ...msg, content: '[AI 助手暂时离线了]', isGenerating: false }
            : msg
        )
      }));
    } finally {
      setIsAiReplying(false);
    }
  };

  const handleCreateRoom = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!newRoomName.trim() || !currentUser.userid) return;

    setIsCreatingRoom(true);
    setCreateRoomError('');
    pendingCreateRoomNameRef.current = newRoomName.trim();
    pendingCreateRoomDescRef.current = newRoomDesc.trim();

    try {
      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), 10000);

      const response = await fetch('/api/createsession', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ userid: currentUser.userid, roomname: newRoomName.trim() }),
        signal: controller.signal
      });

      clearTimeout(timeoutId);

      if (!response.ok) {
        throw new Error('Failed to create room via HTTP');
      }

      const data = await response.json();
      if (data.errormsg) {
        setCreateRoomError(data.errormsg);
        setIsCreatingRoom(false);
        return;
      }

      const { roomid } = data;
      if (!roomid) {
        setCreateRoomError('服务器未返回 roomid');
        setIsCreatingRoom(false);
        return;
      }

      // Send SignalingFromClientPayload using FlatBuffers over WebSocket
      if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
        const buf = encodeSignalingFromClient("subscribe_room", roomid.toString());
        wsRef.current.send(buf);

        // 设置 WebSocket 确认超时
        setTimeout(() => {
          setIsCreatingRoom(prev => {
            if (prev) {
              setCreateRoomError('WebSocket 发送成功，但等待后端 subscribe_ack 回复超时(10秒)');
              return false;
            }
            return prev;
          });
        }, 10000);

      } else {
        setCreateRoomError('WebSocket 未连接，无法发送订阅信令');
        setIsCreatingRoom(false);
      }

    } catch (err: any) {
      if (err.name === 'AbortError') {
        setCreateRoomError('HTTP 接口 /api/createsession 响应超时');
      } else {
        setCreateRoomError(err.message || '创建房间发生未知错误');
      }
      setIsCreatingRoom(false);
    }
    // We do NOT clear input or close modal here. Wait for subscribe_ack!
  };

  const handleGenerateImage = async () => {
    if (!inputValue.trim() || !currentRoom) {
      alert('请先在输入框中输入图片描述');
      return;
    }

    const prompt = inputValue;
    setInputValue('');
    
    const tempId = Date.now().toString();
    const generatingMessage: Message = {
      id: tempId,
      sender: currentUser.username,
      senderId: Number(currentUser.userid),
      content: `[正在生成图片]: ${prompt}`,
      timestamp: new Date(),
      type: 'image',
      isGenerating: true
    };

    setMessages(prev => ({
      ...prev,
      [currentRoom.id]: [...(prev[currentRoom.id] || []), generatingMessage]
    }));
    setIsGenerating(true);

    try {
      // 在 Vite 中，环境变量需要通过 import.meta.env 访问
      // @ts-ignore
      const apiKey = import.meta.env.VITE_GEMINI_API_KEY || import.meta.env.VITE_API_KEY;
      
      if (!apiKey) {
        throw new Error('未找到 Gemini API Key。请在 .env 文件中设置 VITE_GEMINI_API_KEY。');
      }

      const ai = new GoogleGenAI({ apiKey });
      
      const response = await ai.models.generateContent({
        model: 'gemini-3.1-flash-image-preview',
        contents: {
          parts: [
            { text: prompt }
          ]
        },
        config: {
          imageConfig: {
            aspectRatio: "1:1",
            imageSize: "1K"
          }
        }
      });

      let imageUrl = '';
      for (const part of response.candidates?.[0]?.content?.parts || []) {
        if (part.inlineData) {
          imageUrl = `data:${part.inlineData.mimeType || 'image/png'};base64,${part.inlineData.data}`;
          break;
        }
      }

      if (imageUrl) {
        setMessages(prev => ({
          ...prev,
          [currentRoom.id]: (prev[currentRoom.id] || []).map(msg => 
            msg.id === tempId 
              ? { ...msg, content: prompt, imageUrl, isGenerating: false }
              : msg
          )
        }));

        // 生成成功后，也可以将图片消息通过 WebSocket 广播给其他人
        const imgClientMessageId = `msg_${Date.now()}_${Math.random().toString(36).substring(2, 9)}`;
        if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
          const targetRoom = roomsRef.current.find(r => r.id === currentRoom.id);
          const chatType = targetRoom?.chatType === ChatType.Single ? ChatType.Single : ChatType.Group;
          const buf = encodeClientMessage(chatType, currentRoom.id, imgClientMessageId, prompt, 'image', imageUrl);
          wsRef.current.send(buf);
        }

      } else {
        throw new Error('未返回图片数据');
      }
    } catch (error) {
      console.error('Image generation failed:', error);
      setMessages(prev => ({
        ...prev,
        [currentRoom.id]: (prev[currentRoom.id] || []).map(msg => 
          msg.id === tempId 
            ? { ...msg, content: `[图片生成失败]: ${prompt}`, type: 'text', isGenerating: false }
            : msg
        )
      }));
    } finally {
      setIsGenerating(false);
    }
  };

  const handleScroll = () => {
    if (!messageContainerRef.current || !currentRoom) return;
    
    const { scrollTop, scrollHeight, clientHeight } = messageContainerRef.current;
    
    // 记录当前房间的滚动位置
    roomScrollPositionsRef.current[currentRoom.id] = scrollTop;
    
    // 判断是否滚动到了底部（允许 50px 的误差）
    const isBottom = scrollHeight - scrollTop - clientHeight < 50;
    isAtBottomRef.current = isBottom;
    
    // 如果滚动到了底部，清空未读消息计数
    if (isBottom && unreadCount > 0) {
      setUnreadCount(0);
    }

    if (isLoadingHistory) return;

    // 当滚动到距离顶部 50px 以内时，触发加载历史消息
    if (scrollTop < 50) {
      const hasMore = hasMoreMessages[currentRoom.id];
      if (hasMore && wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
        setIsLoadingHistory(true);
        const roomMessages = messages[currentRoom.id] || [];
        const firstMessageId = roomMessages.length > 0 ? roomMessages[0].id : '';
        
        const buf = encodeRequestRoomHistory(currentRoom.id, firstMessageId, 20);
        wsRef.current.send(buf);
      }
    }
  };

  const currentMessages = currentRoom ? (messages[currentRoom.id] || []) : [];

  if (!isLocalLoaded) {
    return (
      <div className="h-screen flex items-center justify-center bg-black text-zinc-400">
        <div className="flex flex-col items-center gap-4">
          <Loader2 className="w-8 h-8 animate-spin text-white" />
          <p>Loading your profile...</p>
        </div>
      </div>
    );
  }

  return (
    <div className="h-screen overflow-hidden flex bg-black text-zinc-300 selection:bg-zinc-800 font-sans">
      {/* Sidebar */}
      <aside className="w-64 flex-shrink-0 bg-[#09090b] flex flex-col">
        {/* User Profile */}
        <div className="p-6 flex items-center justify-between">
          <div className="flex items-center gap-3 overflow-hidden">
            <div className="w-10 h-10 rounded-full bg-zinc-800 flex items-center justify-center text-white font-bold flex-shrink-0">
              {currentUser.username.charAt(0).toUpperCase()}
            </div>
            <div className="truncate">
              <h2 className="text-sm font-bold text-white truncate">{currentUser.username}</h2>
              <div className="flex items-center gap-1.5">
                <div className="w-2 h-2 rounded-full bg-emerald-500"></div>
                <span className="text-xs text-zinc-500">Online</span>
              </div>
            </div>
          </div>
          <button 
            onClick={onLogout}
            className="p-2 text-zinc-500 hover:text-white hover:bg-zinc-900 rounded-full transition-colors"
            title="Sign out"
          >
            <LogOut size={18} />
          </button>
        </div>

        {/* Room List */}
        <div className="flex-1 overflow-y-auto py-4">
          <div className="px-4 mb-4 flex gap-2">
             <form onSubmit={handleJoinRoom} className="relative flex-1">
                <input
                   type="text"
                   value={joinRoomQuery}
                   onChange={e => setJoinRoomQuery(e.target.value)}
                   disabled={isJoiningRoom}
                   placeholder="Search room..."
                   className="w-full bg-zinc-900 border border-zinc-800 text-sm rounded-md py-2 px-3 pr-8 text-zinc-300 placeholder-zinc-500 focus:outline-none focus:border-zinc-600 focus:ring-1 focus:ring-zinc-600 transition-all"
                />
                {isJoiningRoom ? (
                  <div className="absolute right-2 top-2.5">
                    <Loader2 className="w-4 h-4 animate-spin text-zinc-500" />
                  </div>
                ) : (
                  <button type="submit" className="absolute right-2 top-2.5 text-zinc-500 hover:text-white transition-colors" disabled={!joinRoomQuery.trim()}>
                    <Search className="w-4 h-4" />
                  </button>
                )}
             </form>
             <button 
                onClick={() => setShowCreateRoom(true)}
                className="w-9 h-9 bg-zinc-900 hover:bg-zinc-800 border border-zinc-800 rounded-md flex items-center justify-center transition-colors text-zinc-300 hover:text-white shrink-0"
                title="Create Room"
             >
                <Plus size={16} />
             </button>
          </div>
          {joinRoomError && (
             <div className="px-4 mb-3 text-xs text-red-500 flex items-center gap-1">
               <AlertCircle className="w-3 h-3" />
               {joinRoomError}
             </div>
          )}
          <div className="space-y-0.5 px-2">
            {rooms.map(room => (
              <button
                key={room.id}
                onClick={() => setCurrentRoom(room)}
                className={`w-full flex items-center gap-3 px-3 py-3 rounded-md transition-all duration-200 ${
                  currentRoom?.id === room.id 
                    ? 'bg-zinc-900 text-white font-bold' 
                    : 'text-zinc-400 hover:bg-zinc-900/50 hover:text-zinc-200'
                }`}
              >
                {room.chatType === ChatType.Single ? (
                  <div className={`w-5 h-5 rounded flex items-center justify-center font-black text-xs ${currentRoom?.id === room.id ? 'bg-blue-600 text-white' : 'bg-zinc-800 text-zinc-500'}`}>
                    {(room.name || room.id || '?').charAt(0).toUpperCase()}
                  </div>
                ) : (
                  <Hash size={18} className={currentRoom?.id === room.id ? 'text-white' : 'text-zinc-500'} />
                )}
                <span className="truncate">{room.name}</span>
              </button>
            ))}
          </div>
        </div>
      </aside>

      {/* Main Chat Area */}
      <main className="flex-1 flex flex-col bg-black relative">
        {/* Network Status Banner */}
        {connectionState === 'connecting' && (
          <div className="absolute top-0 left-0 right-0 h-8 flex items-center justify-center bg-blue-500/10 border-b border-blue-500/20 z-50">
            <Loader2 className="w-4 h-4 text-blue-400 animate-spin mr-2" />
            <span className="text-xs text-blue-400">连接中...</span>
          </div>
        )}
        {connectionState === 'disconnected' && (
          <div className="absolute top-0 left-0 right-0 h-8 flex items-center justify-center bg-red-500/20 border-b border-red-500/30 z-50">
            <span className="text-xs text-red-500 font-medium">未连接</span>
          </div>
        )}
        {connectionState === 'disconnected' && reconnectAttempt >= 3 && (
          <div className="fixed inset-0 z-50 flex items-center justify-center pointer-events-none">
            <div className="bg-zinc-900 border border-zinc-800 text-white px-6 py-4 rounded-xl shadow-2xl flex flex-col items-center">
              <AlertCircle className="w-8 h-8 text-red-500 mb-2" />
              <p className="font-medium">网络已断开，请检查网络设置</p>
            </div>
          </div>
        )}

        {!currentRoom ? (
          <div className="h-full flex flex-col items-center justify-center text-zinc-500 space-y-6">
            <div className="w-20 h-20 rounded-full bg-gradient-to-tr from-zinc-800 to-zinc-900 flex items-center justify-center shadow-2xl border border-white/5">
              <Hash size={32} className="text-zinc-400" />
            </div>
            <p className="text-lg tracking-wide">Please join or select a room</p>
          </div>
        ) : (
          <>
            {/* Room Header */}
            <header className="h-20 flex items-center justify-between px-8 bg-gradient-to-b from-black via-black/90 to-transparent sticky top-0 z-10 pt-2">
              <div className="flex items-center gap-3">
                {currentRoom.chatType === ChatType.Single ? (
                   <div className="w-8 h-8 rounded-full bg-blue-600 flex items-center justify-center text-white font-bold">
                     {(currentRoom.name || currentRoom.id || '?').charAt(0).toUpperCase()}
                   </div>
                ) : (
                  <div className="w-8 h-8 rounded-full bg-zinc-900 flex items-center justify-center text-zinc-400">
                    <Hash size={18} />
                  </div>
                )}
                <div>
                  <h2 className="text-lg font-bold text-white">{currentRoom.name}</h2>
                  {currentRoom.description && (
                    <p className="text-xs text-zinc-500">{currentRoom.description}</p>
                  )}
                </div>
              </div>
              {/* Network Health Indicator */}
              <div className="flex items-center gap-2">
                {connectionState === 'connected' && rtt !== null && (
                  <div className="flex items-center gap-1.5 px-3 py-1.5 rounded-full bg-zinc-900 border border-zinc-800" title={`Ping: ${rtt}ms`}>
                    <div className={`w-2 h-2 rounded-full ${rtt < 100 ? 'bg-emerald-500' : rtt < 300 ? 'bg-yellow-500' : 'bg-red-500'}`}></div>
                    <span className="text-[10px] text-zinc-500 font-mono">{rtt}ms</span>
                  </div>
                )}
              </div>
            </header>

            {/* Messages */}
            <div 
              className="flex-1 overflow-y-auto p-0 pb-36"
              ref={messageContainerRef}
              onScroll={handleScroll}
            >
              {isLoadingHistory && (
                <div className="flex justify-center py-4 border-b border-zinc-800">
                  <Loader2 className="w-5 h-5 animate-spin text-zinc-500" />
                </div>
              )}
              {currentMessages.length === 0 ? (
                <div className="h-full flex flex-col items-center justify-center text-zinc-500 space-y-6">
                  <div className="w-20 h-20 rounded-full bg-gradient-to-tr from-zinc-800 to-zinc-900 flex items-center justify-center shadow-2xl border border-white/5">
                    <MessageSquare size={32} className="text-zinc-400" />
                  </div>
                  <p className="text-lg tracking-wide">Start the conversation</p>
                </div>
              ) : (
                <div className="flex flex-col gap-6 px-6 py-4">
              {currentMessages.map((msg) => {
                const isMe = msg.sender === currentUser.username;
                const isAI = msg.sender === 'AI Assistant';
                return (
                  <div key={msg.id} className={`flex gap-3 w-full ${isMe ? 'flex-row-reverse' : 'flex-row'} animate-in fade-in slide-in-from-bottom-2 duration-300`}>
                    {/* Avatar */}
                    <div 
                      onClick={() => !isMe && !isAI && handleUserClick({id: msg.senderId || msg.sender, name: msg.sender})}
                      className={`w-10 h-10 rounded-full flex-shrink-0 flex items-center justify-center font-bold text-white shadow-sm ${isAI ? 'bg-blue-600' : 'bg-zinc-800'} ${!isMe && !isAI ? 'cursor-pointer hover:ring-2 hover:ring-white/20 transition-all' : ''}`}
                      title={!isMe ? "Direct Message" : undefined}
                    >
                      {isAI ? <Sparkles size={18} /> : msg.sender.charAt(0).toUpperCase()}
                    </div>
                    
                    {/* Content */}
                    <div className={`flex flex-col max-w-[75%] ${isMe ? 'items-end' : 'items-start'}`}>
                      <div className={`flex items-baseline gap-2 mb-1.5 px-1 ${isMe ? 'flex-row-reverse' : 'flex-row'}`}>
                        <span className="text-sm font-bold text-zinc-300">
                          {msg.sender}
                        </span>
                        <span className="text-xs text-zinc-500">
                          {new Date(msg.timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}
                        </span>
                      </div>
                      
                      <div 
                        onContextMenu={(e) => { e.preventDefault(); setReplyingTo(msg); }}
                        className={`relative px-4 py-3 text-base leading-relaxed shadow-sm group ${
                        isMe 
                          ? 'bg-blue-600 text-white rounded-2xl rounded-tr-sm' 
                          : isAI
                            ? 'bg-zinc-900 border border-blue-500/30 text-zinc-200 rounded-2xl rounded-tl-sm'
                            : 'bg-zinc-900 border border-zinc-800 text-zinc-200 rounded-2xl rounded-tl-sm'
                      }`}>
                        {msg.replyTo && (
                          <div className={`text-xs px-2 py-1 mb-2 rounded border-l-2 bg-black/20 ${isMe ? 'border-blue-300 text-blue-100' : 'border-zinc-500 text-zinc-400'}`}>
                            Replied to message
                          </div>
                        )}
                        {msg.type === 'text' && (
                          <div className={`flex items-center gap-2 ${isMe ? 'flex-row-reverse' : 'flex-row'}`}>
                            {msg.isGenerating && <Loader2 size={14} className="animate-spin text-blue-400" />}
                            <p className="whitespace-pre-wrap break-words">{msg.content}</p>
                            {isMe && msg.status === 'sending' && (
                              <Loader2 size={14} className="animate-spin text-blue-200 ml-1 flex-shrink-0" />
                            )}
                            {isMe && msg.status === 'failed' && (
                              <div className="flex items-center ml-1 flex-shrink-0 gap-1">
                                <AlertCircle size={14} className="text-red-300" title="Failed to send" />
                                <button 
                                  onClick={() => sendClientMessage(currentRoom!.id, msg.content, msg.clientMessageId!, true)}
                                  className="text-blue-200 hover:text-white transition-colors p-1"
                                  title="Retry"
                                >
                                  <RefreshCw size={12} />
                                </button>
                              </div>
                            )}
                          </div>
                        )}
                        
                        {msg.type === 'image' && (
                          <div className="space-y-3">
                            <p className={`text-sm flex items-center gap-1.5 ${isMe ? 'text-blue-100' : 'text-zinc-400'}`}>
                              <Sparkles size={14} />
                              {msg.content}
                            </p>
                            {msg.isGenerating ? (
                              <div className="w-64 h-64 bg-zinc-900/50 rounded-xl flex flex-col items-center justify-center gap-3 animate-pulse border border-zinc-800/50">
                                <Loader2 className="w-8 h-8 animate-spin text-zinc-500" />
                                <span className="text-sm font-medium text-zinc-500">Generating...</span>
                              </div>
                            ) : msg.imageUrl ? (
                              <div className="relative inline-block">
                                <img 
                                  src={msg.imageUrl} 
                                  alt={msg.content} 
                                  className={`rounded-xl max-w-sm h-auto border border-zinc-800/50 cursor-pointer shadow-md ${msg.status === 'sending' ? 'opacity-70' : ''}`}
                                  referrerPolicy="no-referrer"
                                  onClick={() => window.open(msg.imageUrl, '_blank')}
                                />
                                {isMe && msg.status === 'sending' && (
                                  <div className="absolute top-2 right-2 bg-black/50 rounded-full p-1">
                                    <Loader2 size={16} className="animate-spin text-white" />
                                  </div>
                                )}
                                {isMe && msg.status === 'failed' && (
                                  <div className="absolute top-2 right-2 bg-black/50 rounded-full p-1 flex items-center gap-1">
                                    <AlertCircle size={16} className="text-red-500" title="Failed to send" />
                                    <button 
                                      onClick={() => sendClientMessage(currentRoom!.id, msg.content, msg.clientMessageId!, true, 'image', msg.imageUrl)}
                                      className="text-white hover:text-red-400 transition-colors bg-black/50 hover:bg-black/80 rounded-full p-1"
                                      title="Retry"
                                    >
                                      <RefreshCw size={14} />
                                    </button>
                                  </div>
                                )}
                              </div>
                            ) : null}
                          </div>
                        )}
                      </div>
                    </div>
                  </div>
                );
              })}
            </div>
          )}
          <div ref={messagesEndRef} className="h-4" />
        </div>

        {/* Unread Messages Badge */}
        {unreadCount > 0 && (
          <button
            onClick={() => {
              scrollToBottom();
              setUnreadCount(0);
            }}
            className="absolute bottom-32 right-8 bg-white text-black px-4 py-2 rounded-full shadow-lg flex items-center gap-2 hover:bg-zinc-200 transition-all z-20 font-bold text-sm"
          >
            <span className="flex h-2 w-2 relative">
              <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-black opacity-75"></span>
              <span className="relative inline-flex rounded-full h-2 w-2 bg-black"></span>
            </span>
            <span>{unreadCount} new messages</span>
          </button>
        )}

        {/* Input Area */}
        <div className="absolute bottom-0 left-0 right-0 p-6 bg-gradient-to-t from-black via-black/90 to-transparent pointer-events-none">
          <div className="max-w-4xl mx-auto pointer-events-auto">
            {replyingTo && (
              <div className="mb-2 mx-4 p-3 bg-zinc-900 border border-zinc-800 rounded-xl relative animate-in fade-in slide-in-from-bottom-2">
                <div className="flex items-center justify-between">
                  <span className="text-sm font-bold text-blue-400">Replying to {replyingTo.sender}</span>
                  <button onClick={() => setReplyingTo(null)} className="text-zinc-500 hover:text-zinc-300">
                    <X size={16} />
                  </button>
                </div>
                <p className="text-zinc-400 text-sm mt-1 truncate">{replyingTo.content}</p>
              </div>
            )}
            <form onSubmit={handleSendMessage} className="flex items-end gap-2 p-2 bg-zinc-900/80 backdrop-blur-2xl border border-white/10 rounded-[2.5rem] shadow-2xl">
              <div className="flex-1 relative group">
                <input
                  type="text"
                  value={inputValue}
                  onChange={(e) => setInputValue(e.target.value)}
                  placeholder={currentRoom ? `Message #${currentRoom.name}` : 'Select a channel...'}
                  className="w-full bg-transparent text-white pl-6 pr-4 py-4 focus:outline-none placeholder:text-zinc-500 text-base"
                  disabled={isGenerating || !currentRoom}
                />
              </div>
              
              <button
                type="button"
                onClick={handleGenerateImage}
                disabled={isGenerating || !inputValue.trim() || !currentRoom}
                title="Generate Image with AI"
                className="p-3.5 text-zinc-400 hover:text-white bg-transparent hover:bg-white/5 rounded-full transition-colors disabled:opacity-50 disabled:cursor-not-allowed flex-shrink-0"
              >
                <ImageIcon size={22} />
              </button>
              
              <button
                type="submit"
                disabled={isGenerating || !inputValue.trim() || !currentRoom}
                className="p-3.5 bg-white hover:bg-zinc-200 text-black rounded-full transition-colors disabled:opacity-50 disabled:cursor-not-allowed flex-shrink-0 font-bold shadow-md"
              >
                <Send size={20} className="ml-0.5" />
              </button>
            </form>
          </div>
        </div>
        </>
        )}
      </main>

      {/* Create Room Modal */}
      {showCreateRoom && (
        <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/80 backdrop-blur-sm">
          <div className="bg-black border border-zinc-800 rounded-2xl p-6 w-full max-w-md shadow-2xl">
            <div className="flex items-center justify-between mb-6">
              <h3 className="text-xl font-bold text-white flex items-center gap-2">
                Create Channel
              </h3>
              <button 
                onClick={() => setShowCreateRoom(false)}
                className="text-zinc-500 hover:text-white p-2 rounded-full hover:bg-zinc-900 transition-colors"
              >
                <X size={20} />
              </button>
            </div>
            
            <form onSubmit={handleCreateRoom} className="space-y-5">
              <div className="space-y-2">
                <label className="block text-sm font-bold text-zinc-400">Name</label>
                <input
                  type="text"
                  required
                  disabled={isCreatingRoom}
                  value={newRoomName}
                  onChange={(e) => setNewRoomName(e.target.value)}
                  className="block w-full px-4 py-3 border border-zinc-800 rounded-md bg-black focus:border-blue-500 focus:ring-1 focus:ring-blue-500 transition-colors text-white placeholder:text-zinc-600 outline-none disabled:opacity-50"
                  placeholder="e.g. gaming"
                />
              </div>
              <div className="space-y-2">
                <label className="block text-sm font-bold text-zinc-400">Description (Optional)</label>
                <input
                  type="text"
                  disabled={isCreatingRoom}
                  value={newRoomDesc}
                  onChange={(e) => setNewRoomDesc(e.target.value)}
                  className="block w-full px-4 py-3 border border-zinc-800 rounded-md bg-black focus:border-blue-500 focus:ring-1 focus:ring-blue-500 transition-colors text-white placeholder:text-zinc-600 outline-none disabled:opacity-50"
                  placeholder="What's this channel about?"
                />
              </div>
              {createRoomError && (
                 <div className="text-xs text-red-500 flex items-center gap-1">
                   <AlertCircle className="w-3 h-3" />
                   {createRoomError}
                 </div>
              )}
              <div className="pt-4 flex gap-3">
                <button
                  type="button"
                  disabled={isCreatingRoom}
                  onClick={() => setShowCreateRoom(false)}
                  className="flex-1 py-3 px-4 bg-transparent hover:bg-zinc-900 border border-zinc-800 text-white rounded-full font-bold transition-colors disabled:opacity-50"
                >
                  Cancel
                </button>
                <button
                  type="submit"
                  disabled={isCreatingRoom || !newRoomName.trim()}
                  className="flex-1 py-3 px-4 bg-white hover:bg-zinc-200 text-black rounded-full font-bold transition-colors flex items-center justify-center gap-2 disabled:opacity-50"
                >
                  {isCreatingRoom ? <Loader2 className="w-5 h-5 animate-spin" /> : 'Create'}
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* User Profile Modal */}
      {selectedUser && (
        <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/80 backdrop-blur-sm" onClick={() => setSelectedUser(null)}>
          <div className="bg-black border border-zinc-800 rounded-2xl p-8 w-full max-w-sm shadow-2xl flex flex-col items-center gap-6" onClick={(e) => e.stopPropagation()}>
            <div className="w-24 h-24 rounded-full bg-blue-600 flex items-center justify-center text-4xl font-bold text-white shadow-lg">
              {selectedUser.name.charAt(0).toUpperCase()}
            </div>
            
            <div className="text-center">
              <h3 className="text-2xl font-bold text-white mb-1">
                {selectedUser.name}
              </h3>
              <p className="text-zinc-500 text-sm">Active Member</p>
            </div>
            
            <div className="w-full pt-4">
              <button
                onClick={() => handleStartDirectMessage(selectedUser)}
                className="w-full py-3 px-4 bg-white hover:bg-zinc-200 text-black rounded-full font-bold transition-colors flex items-center justify-center gap-2"
              >
                <MessageSquare size={18} />
                Message
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
