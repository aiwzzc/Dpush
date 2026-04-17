import React, { useState, useRef, useEffect } from 'react';
import { User, Message, Room } from '../types';
import { Send, Image as ImageIcon, LogOut, Sparkles, Loader2, Hash, Plus, X, Users, MessageSquare, AlertCircle, RefreshCw } from 'lucide-react';
import { GoogleGenAI } from '@google/genai';
import * as flatbuffers from 'flatbuffers';
import { RootMessage, AnyPayload, ServerMessagePayload, RequestMessagePayload, MessageAckPayload, MsgContentType, HelloMessagePayload } from '../generated/chat_app';
import { encodeClientMessage, encodePullMissingMessages, encodeRequestRoomHistory } from '../utils/fb-helper';

const DEFAULT_ROOMS: Room[] = [
  { id: 'general', name: '大厅', description: '所有人都在这里畅所欲言' },
  { id: 'tech', name: '技术交流', description: '讨论编程、架构与前沿技术' },
  { id: 'random', name: '摸鱼专区', description: '随便聊聊，放松一下' },
];

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

  const messagesEndRef = useRef<HTMLDivElement>(null);
  const messageContainerRef = useRef<HTMLDivElement>(null);
  const isAtBottomRef = useRef<boolean>(true);
  const currentRoomIdRef = useRef<string | null>(null);
  const roomScrollPositionsRef = useRef<Record<string, number>>({});
  const networkRetryTimersRef = useRef<Record<string, NodeJS.Timeout>>({});
  const businessTimeoutTimersRef = useRef<Record<string, NodeJS.Timeout>>({});
  const roomMaxServerMsgIdRef = useRef<Record<string, number>>({});
  const pendingMessagesRef = useRef<Record<string, Record<number, any>>>({});

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

  // 初始化 WebSocket 连接
  useEffect(() => {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    // 假设后端的 websocket 路径是 /ws
    const wsUrl = `${protocol}//${window.location.host}/ws`;
    const socket = new WebSocket(wsUrl);

    socket.binaryType = 'arraybuffer';

    socket.onopen = () => {
      console.log('WebSocket connected');
    };

    socket.onmessage = async (event) => {
      try {
        if (event.data instanceof ArrayBuffer || event.data instanceof Blob) {
          // 处理 FlatBuffers 消息
          const arrayBuffer = event.data instanceof Blob ? await event.data.arrayBuffer() : event.data;
          const buf = new flatbuffers.ByteBuffer(new Uint8Array(arrayBuffer));
          const root = RootMessage.getRootAsRootMessage(buf);
          const payloadType = root.payloadType();

          if (payloadType === AnyPayload.ServerMessagePayload) {
            const payload = root.payload(new ServerMessagePayload()) as ServerMessagePayload;
            const roomId = payload.roomId() || 'general';
            const messagesLen = payload.messagesLength();
            
            let messagesToAdd: Message[] = [];
            let hasNewMessages = false;

            const processMsg = (msgRaw: any, sMsgId: number) => {
              const parsedMsg: Message = {
                id: msgRaw.id || Date.now().toString(),
                clientMessageId: msgRaw.clientMessageId,
                serverMessageId: sMsgId,
                sender: msgRaw.user?.username || 'Unknown',
                content: msgRaw.content,
                timestamp: parseTimestamp(msgRaw.timestamp),
                type: msgRaw.msgType || 'text',
                imageUrl: msgRaw.imageUrl
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
            for (let i = 0; i < messagesLen; i++) {
              const msg = payload.messages(i);
              if (msg) {
                incomingMessages.push({
                  id: msg.serverMessageId().toString(),
                  clientMessageId: msg.clientMessageId(),
                  serverMessageId: Number(msg.serverMessageId()),
                  content: msg.content(),
                  user: { username: msg.user()?.username() || 'Unknown' },
                  timestamp: Number(msg.timestamp()),
                  msgType: msg.msgType() === MsgContentType.Image ? 'image' : 'text',
                  imageUrl: msg.imageUrl() || undefined
                });
              }
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
          } else if (payloadType === AnyPayload.HelloMessagePayload) {
            const payload = root.payload(new HelloMessagePayload()) as HelloMessagePayload;
            const me = payload.me();
            if (me) {
              setCurrentUser({ userid: Number(me.userid()), username: me.username() || 'Unknown' });
            }
            
            const roomsLen = payload.roomsLength();
            const parsedRooms: Room[] = [];
            const initialMessages: Record<string, Message[]> = {};
            const initialHasMore: Record<string, boolean> = {};

            for (let i = 0; i < roomsLen; i++) {
              const r = payload.rooms(i);
              if (!r) continue;
              const roomId = r.roomId() || '';
              parsedRooms.push({
                id: roomId,
                name: r.roomname() || '',
                description: ''
              });

              initialHasMore[roomId] = r.hasMoreMessages();
              
              const msgsLen = r.messagesLength();
              const msgs: Message[] = [];
              for (let j = 0; j < msgsLen; j++) {
                const msg = r.messages(j);
                if (!msg) continue;
                msgs.push({
                  id: msg.id() || Date.now().toString(),
                  sender: msg.user()?.username() || 'Unknown',
                  content: msg.content() || '',
                  timestamp: parseTimestamp(Number(msg.timestamp())),
                  type: msg.msgType() === MsgContentType.Image ? 'image' : 'text',
                  imageUrl: msg.imageUrl() || undefined
                });
              }
              initialMessages[roomId] = msgs.sort((a, b) => a.timestamp.getTime() - b.timestamp.getTime());
            }

            setRooms(parsedRooms);
            if (parsedRooms.length > 0 && !currentRoomIdRef.current) {
              setCurrentRoom(parsedRooms[0]);
            }
            setMessages(prev => ({ ...prev, ...initialMessages }));
            setHasMoreMessages(prev => ({ ...prev, ...initialHasMore }));
            setTimeout(scrollToBottom, 100);
          }
          return;
        }

        // 处理 JSON 消息 (如 serverCreateRoom 等不在 Schema 中的消息)
        const data = JSON.parse(event.data);
        
        // 处理服务器广播的新建房间消息
        if (data.type === 'serverCreateRoom' && data.payload) {
          const newRoom: Room = {
            id: data.payload.roomId,
            name: data.payload.roomName,
            description: '' // 后端目前没有 description 字段
          };
          
          setRooms(prev => {
            // 避免重复添加
            if (prev.some(r => r.id === newRoom.id)) return prev;
            return [...prev, newRoom];
          });
        }
      } catch (e) {
        console.error('Failed to parse message', e);
      }
    };

    socket.onclose = () => {
      console.log('WebSocket disconnected');
    };

    setWs(socket);

    return () => {
      socket.close();
    };
  }, [user.username]);

  const sendClientMessage = (roomId: string, content: string, clientMessageId: string, isRetry = false, msgType: 'text' | 'image' = 'text', imageUrl?: string) => {
    const sendToWs = () => {
      if (ws && ws.readyState === WebSocket.OPEN) {
        const buf = encodeClientMessage(roomId, clientMessageId, content, msgType, imageUrl);
        ws.send(buf);
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

  const handleSendMessage = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!inputValue.trim() || !currentRoom) return;

    const messageContent = inputValue;
    setInputValue('');

    const clientMessageId = `msg_${Date.now()}_${Math.random().toString(36).substring(2, 9)}`;
    sendClientMessage(currentRoom.id, messageContent, clientMessageId);

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
      if (ws && ws.readyState === WebSocket.OPEN) {
        const buf = encodeClientMessage(roomId, aiClientMessageId, aiReply, 'text');
        ws.send(buf);
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

  const handleCreateRoom = (e: React.FormEvent) => {
    e.preventDefault();
    if (!newRoomName.trim()) return;

    // 构造发给后端的创建房间请求
    const createRoomReq = {
      type: 'clientCreateRoom',
      payload: {
        roomName: newRoomName.trim()
      }
    };

    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(createRoomReq));
    } else {
      console.warn('WebSocket is not connected. Room creation request not sent.');
    }

    // 乐观更新 UI：我们先不在这里更新房间列表，而是等待后端广播 serverCreateRoom
    // 这样可以确保房间 ID 是后端生成的真实 ID
    
    setNewRoomName('');
    setNewRoomDesc('');
    setShowCreateRoom(false);
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
        if (ws && ws.readyState === WebSocket.OPEN) {
          const buf = encodeClientMessage(currentRoom.id, imgClientMessageId, prompt, 'image', imageUrl);
          ws.send(buf);
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
      if (hasMore && ws && ws.readyState === WebSocket.OPEN) {
        setIsLoadingHistory(true);
        const roomMessages = messages[currentRoom.id] || [];
        const firstMessageId = roomMessages.length > 0 ? roomMessages[0].id : '';
        
        const buf = encodeRequestRoomHistory(currentRoom.id, firstMessageId, 20);
        ws.send(buf);
      }
    }
  };

  const currentMessages = currentRoom ? (messages[currentRoom.id] || []) : [];

  if (!currentRoom && rooms.length === 0) {
    return (
      <div className="h-screen flex items-center justify-center bg-black text-zinc-400">
        <div className="flex flex-col items-center gap-4">
          <Loader2 className="w-8 h-8 animate-spin text-white" />
          <p>Connecting to XChat...</p>
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
          <div className="px-4 mb-2 flex items-center justify-between group">
            <h3 className="text-xs font-bold text-zinc-500 uppercase tracking-wider">Channels</h3>
            <button 
              onClick={() => setShowCreateRoom(true)}
              className="text-zinc-500 hover:text-white p-1 rounded-full hover:bg-zinc-900 transition-colors opacity-0 group-hover:opacity-100"
              title="Create Channel"
            >
              <Plus size={16} />
            </button>
          </div>
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
                <Hash size={18} className={currentRoom?.id === room.id ? 'text-white' : 'text-zinc-500'} />
                <span className="truncate">{room.name}</span>
              </button>
            ))}
          </div>
        </div>
      </aside>

      {/* Main Chat Area */}
      <main className="flex-1 flex flex-col bg-black relative">
        {/* Room Header */}
        <header className="h-20 flex items-center px-8 bg-gradient-to-b from-black via-black/90 to-transparent sticky top-0 z-10">
          <div className="flex items-center gap-3">
            <div className="w-8 h-8 rounded-full bg-zinc-900 flex items-center justify-center text-zinc-400">
              <Hash size={18} />
            </div>
            <div>
              <h2 className="text-lg font-bold text-white">{currentRoom?.name}</h2>
              {currentRoom?.description && (
                <p className="text-xs text-zinc-500">{currentRoom.description}</p>
              )}
            </div>
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
                    <div className={`w-10 h-10 rounded-full flex-shrink-0 flex items-center justify-center font-bold text-white shadow-sm ${isAI ? 'bg-blue-600' : 'bg-zinc-800'}`}>
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
                      
                      <div className={`relative px-4 py-3 text-base leading-relaxed shadow-sm ${
                        isMe 
                          ? 'bg-blue-600 text-white rounded-2xl rounded-tr-sm' 
                          : isAI
                            ? 'bg-zinc-900 border border-blue-500/30 text-zinc-200 rounded-2xl rounded-tl-sm'
                            : 'bg-zinc-900 border border-zinc-800 text-zinc-200 rounded-2xl rounded-tl-sm'
                      }`}>
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
                  value={newRoomName}
                  onChange={(e) => setNewRoomName(e.target.value)}
                  className="block w-full px-4 py-3 border border-zinc-800 rounded-md bg-black focus:border-blue-500 focus:ring-1 focus:ring-blue-500 transition-colors text-white placeholder:text-zinc-600 outline-none"
                  placeholder="e.g. gaming"
                />
              </div>
              <div className="space-y-2">
                <label className="block text-sm font-bold text-zinc-400">Description (Optional)</label>
                <input
                  type="text"
                  value={newRoomDesc}
                  onChange={(e) => setNewRoomDesc(e.target.value)}
                  className="block w-full px-4 py-3 border border-zinc-800 rounded-md bg-black focus:border-blue-500 focus:ring-1 focus:ring-blue-500 transition-colors text-white placeholder:text-zinc-600 outline-none"
                  placeholder="What's this channel about?"
                />
              </div>
              <div className="pt-4 flex gap-3">
                <button
                  type="button"
                  onClick={() => setShowCreateRoom(false)}
                  className="flex-1 py-3 px-4 bg-transparent hover:bg-zinc-900 text-white rounded-full font-bold transition-colors border border-zinc-800"
                >
                  Cancel
                </button>
                <button
                  type="submit"
                  className="flex-1 py-3 px-4 bg-white hover:bg-zinc-200 text-black rounded-full font-bold transition-colors"
                >
                  Create
                </button>
              </div>
            </form>
          </div>
        </div>
      )}
    </div>
  );
}
