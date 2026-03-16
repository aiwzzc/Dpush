import React, { useState, useRef, useEffect } from 'react';
import { User, Message, Room } from '../types';
import { Send, Image as ImageIcon, LogOut, Sparkles, Loader2, Hash, Plus, X, Users, MessageSquare, AlertCircle, RefreshCw } from 'lucide-react';
import { GoogleGenAI } from '@google/genai';

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

    socket.onopen = () => {
      console.log('WebSocket connected');
      // 发送认证或上线消息
      socket.send(JSON.stringify({ type: 'auth', user: user.username }));
    };

    socket.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        
        // 处理初始化的 hello 消息
        if (data.type === 'hello' && data.payload) {
          if (data.payload.me) {
            setCurrentUser(data.payload.me);
          }
          
          const incomingRooms = data.payload.rooms || [];
          const parsedRooms: Room[] = incomingRooms.map((r: any) => ({
            id: r.id,
            name: r.name,
            description: '' // 后端目前没有 description 字段，可以留空
          }));
          
          setRooms(parsedRooms);
          if (parsedRooms.length > 0 && !currentRoomIdRef.current) {
            setCurrentRoom(parsedRooms[0]);
          }

          const initialMessages: Record<string, Message[]> = {};
          const initialHasMore: Record<string, boolean> = {};

          incomingRooms.forEach((r: any) => {
            initialHasMore[r.id] = r.hasMoreMessage;
            initialMessages[r.id] = (r.messages || [])
              .map((msg: any) => ({
                id: msg.id || Date.now().toString(),
                sender: msg.user?.username || 'Unknown',
                content: msg.content,
                timestamp: parseTimestamp(msg.timestamp),
                type: msg.msgType || 'text',
                imageUrl: msg.imageUrl
              }))
              .sort((a: Message, b: Message) => a.timestamp.getTime() - b.timestamp.getTime());
          });

          setMessages(prev => ({ ...prev, ...initialMessages }));
          setHasMoreMessages(prev => ({ ...prev, ...initialHasMore }));
          
          // 初始加载完成后滚动到底部
          setTimeout(scrollToBottom, 100);
        }
        
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
        
        // 处理接收到的消息
        if (data.type === 'ServerMessage' && data.payload) {
          const roomId = data.payload.roomId || 'general';
          const incomingMessages = data.payload.message || [];
          
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

          // 按 serverMessageId 排序，以防后端批量下发时乱序
          const sortedMessages = [...incomingMessages].sort((a, b) => {
            const idA = a.serverMessageId !== undefined ? Number(a.serverMessageId) : 0;
            const idB = b.serverMessageId !== undefined ? Number(b.serverMessageId) : 0;
            return idA - idB;
          });

          sortedMessages.forEach((msgRaw: any) => {
            const serverMessageId = msgRaw.serverMessageId !== undefined ? Number(msgRaw.serverMessageId) : NaN;

            if (!isNaN(serverMessageId)) {
              const currentMax = roomMaxServerMsgIdRef.current[roomId];
              
              if (currentMax === undefined) {
                // 第一次收到该房间的消息，初始化 maxId
                processMsg(msgRaw, serverMessageId);
              } else if (serverMessageId <= currentMax) {
                // 收到重复或旧消息，忽略
                console.log(`Ignored duplicate or old message ${serverMessageId} for room ${roomId}`);
              } else if (serverMessageId === currentMax + 1) {
                // 收到期望的下一条消息
                processMsg(msgRaw, serverMessageId);
              } else {
                // 发现 Gap (serverMessageId > currentMax + 1)
                if (!pendingMessagesRef.current[roomId]) {
                  pendingMessagesRef.current[roomId] = {};
                }
                // 暂存这条超前的消息
                pendingMessagesRef.current[roomId][serverMessageId] = msgRaw;
                
                const missingIds: number[] = [];
                for (let id = currentMax + 1; id < serverMessageId; id++) {
                  if (!pendingMessagesRef.current[roomId][id]) {
                    missingIds.push(id);
                  }
                }
                
                if (missingIds.length > 0) {
                  const delay = Math.floor(Math.random() * 1900) + 100; // 100ms - 2000ms
                  setTimeout(() => {
                    const currentMaxNow = roomMaxServerMsgIdRef.current[roomId] || 0;
                    // 检查定时器触发时，这些消息是否仍然缺失
                    const stillMissing = missingIds.filter(id => 
                      currentMaxNow < id && !pendingMessagesRef.current[roomId]?.[id]
                    );
                    
                    if (stillMissing.length > 0 && socket.readyState === WebSocket.OPEN) {
                      const pullReq = {
                        type: 'PullMissingMessages',
                        payload: {
                          roomId: roomId,
                          missingMessageIds: stillMissing
                        }
                      };
                      socket.send(JSON.stringify(pullReq));
                    }
                  }, delay);
                }
              }
            } else {
              // 兼容没有 serverMessageId 的情况
              processMsg(msgRaw, NaN);
            }
          });

          if (hasNewMessages) {
            setMessages(prev => {
              const existingRoomMsgs = prev[roomId] || [];
              const newRoomMsgs = [...existingRoomMsgs];
              
              messagesToAdd.forEach(newMsg => {
                // 如果是我们自己发的消息，收到 ServerMessage 说明业务处理成功
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
                  // 替换乐观更新的消息，并标记为成功（取消转圈圈）
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
        }

        // 处理历史消息
        if (data.type === 'RequestMessage' && data.payload) {
          const roomId = data.payload.roomId;
          const incomingMessages = data.payload.message || [];
          const hasMore = data.payload.hasMoreMessages;

          const parsedHistoryMessages: Message[] = incomingMessages.map((msg: any) => ({
            id: msg.id || Date.now().toString(),
            clientMessageId: msg.clientMessageId,
            sender: msg.user?.username || 'Unknown',
            content: msg.content,
            timestamp: parseTimestamp(msg.timestamp),
            type: msg.msgType || 'text',
            imageUrl: msg.imageUrl
          }));

          setHasMoreMessages(prev => ({
            ...prev,
            [roomId]: hasMore
          }));

          // 记录更新前的高度和滚动位置，用于保持视口稳定
          const container = messageContainerRef.current;
          const previousScrollHeight = container ? container.scrollHeight : 0;
          const previousScrollTop = container ? container.scrollTop : 0;

          setMessages(prev => {
            const existingMessages = prev[roomId] || [];
            // 将历史消息加到前面
            const updatedRoomMessages = [...parsedHistoryMessages, ...existingMessages];
            // 确保消息始终按时间戳升序排列
            updatedRoomMessages.sort((a, b) => a.timestamp.getTime() - b.timestamp.getTime());
            return {
              ...prev,
              [roomId]: updatedRoomMessages
            };
          });
          
          // 恢复滚动位置，使得用户感觉画面没有跳动
          setTimeout(() => {
            if (container) {
              const newScrollHeight = container.scrollHeight;
              // 新的高度减去旧的高度，就是新增内容的高度。将滚动条向下移动这个高度，就能保持原来的内容在视口中
              container.scrollTop = previousScrollTop + (newScrollHeight - previousScrollHeight);
            }
            setIsLoadingHistory(false);
          }, 0);
        }

        // 处理消息 ACK
        if (data.type === 'MessageAck' && data.payload) {
          const { clientMessageId, status } = data.payload;
          
          if (status === 'SUCCESS') {
            // 收到网关 ACK，停止网络重传定时器，但保持 sending 状态（转圈圈）
            if (networkRetryTimersRef.current[clientMessageId]) {
              clearInterval(networkRetryTimersRef.current[clientMessageId]);
              delete networkRetryTimersRef.current[clientMessageId];
            }
          } else {
            // 如果网关明确返回失败，直接标记为 failed
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

  const sendClientMessage = (roomId: string, content: string, clientMessageId: string, isRetry = false) => {
    const newMsgObj = {
      clientMessageId,
      type: 'ClientMessage',
      payload: {
        roomId,
        messages: [{ content, msgType: 'text' }]
      }
    };

    const sendToWs = () => {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify(newMsgObj));
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
      const aiMsgObj = {
        type: 'ClientMessage',
        payload: {
          roomId: roomId,
          messages: [
            {
              content: aiReply,
              msgType: 'text'
            }
          ]
        }
      };
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify(aiMsgObj));
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
        const newMsgObj = {
          type: 'ClientMessage',
          payload: {
            roomId: currentRoom.id,
            messages: [
              {
                content: prompt,
                imageUrl: imageUrl,
                msgType: 'image'
              }
            ]
          }
        };
        if (ws && ws.readyState === WebSocket.OPEN) {
          ws.send(JSON.stringify(newMsgObj));
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
        
        const requestMsg = {
          type: 'RequestRoomHistory',
          payload: {
            roomId: currentRoom.id,
            firstMessageId: firstMessageId,
            count: 20
          }
        };
        ws.send(JSON.stringify(requestMsg));
      }
    }
  };

  const currentMessages = currentRoom ? (messages[currentRoom.id] || []) : [];

  if (!currentRoom && rooms.length === 0) {
    return (
      <div className="h-screen flex items-center justify-center bg-[#0B0F19] text-slate-300">
        <div className="flex flex-col items-center gap-4">
          <Loader2 className="w-8 h-8 animate-spin text-indigo-500" />
          <p>正在连接服务器并加载房间列表...</p>
        </div>
      </div>
    );
  }

  return (
    <div className="h-screen overflow-hidden flex bg-[#0B0F19] text-slate-300 selection:bg-indigo-500/30">
      {/* Sidebar */}
      <aside className="w-64 flex-shrink-0 bg-[#0B0F19] border-r border-white/5 flex flex-col">
        {/* User Profile */}
        <div className="p-4 border-b border-white/5 flex items-center justify-between">
          <div className="flex items-center gap-3 overflow-hidden">
            <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-indigo-500 to-purple-600 flex items-center justify-center text-white font-bold shadow-lg flex-shrink-0">
              {currentUser.username.charAt(0).toUpperCase()}
            </div>
            <div className="truncate">
              <h2 className="text-sm font-bold text-white truncate">{currentUser.username}</h2>
              <div className="flex items-center gap-1.5">
                <div className="w-2 h-2 rounded-full bg-emerald-500 shadow-[0_0_8px_rgba(16,185,129,0.5)]"></div>
                <span className="text-xs text-slate-400">在线</span>
              </div>
            </div>
          </div>
          <button 
            onClick={onLogout}
            className="p-2 text-slate-400 hover:text-red-400 hover:bg-white/5 rounded-lg transition-colors"
            title="退出登录"
          >
            <LogOut size={18} />
          </button>
        </div>

        {/* Room List */}
        <div className="flex-1 overflow-y-auto py-4">
          <div className="px-4 mb-2 flex items-center justify-between group">
            <h3 className="text-xs font-semibold text-slate-500 uppercase tracking-wider">聊天频道</h3>
            <button 
              onClick={() => setShowCreateRoom(true)}
              className="text-slate-400 hover:text-white p-1 rounded hover:bg-white/10 transition-colors opacity-0 group-hover:opacity-100"
              title="创建频道"
            >
              <Plus size={14} />
            </button>
          </div>
          <div className="space-y-0.5 px-2">
            {rooms.map(room => (
              <button
                key={room.id}
                onClick={() => setCurrentRoom(room)}
                className={`w-full flex items-center gap-3 px-3 py-2 rounded-lg transition-all duration-200 ${
                  currentRoom?.id === room.id 
                    ? 'bg-indigo-500/10 text-indigo-400' 
                    : 'text-slate-400 hover:bg-white/5 hover:text-slate-200'
                }`}
              >
                <Hash size={18} className={currentRoom?.id === room.id ? 'text-indigo-400' : 'text-slate-500'} />
                <span className="font-medium truncate">{room.name}</span>
              </button>
            ))}
          </div>
        </div>
      </aside>

      {/* Main Chat Area */}
      <main className="flex-1 flex flex-col bg-[#131825] relative">
        {/* Room Header */}
        <header className="h-16 border-b border-white/5 flex items-center px-6 bg-[#131825]/80 backdrop-blur-md sticky top-0 z-10">
          <div className="flex items-center gap-3">
            <div className="w-8 h-8 rounded-lg bg-white/5 flex items-center justify-center text-slate-400">
              <Hash size={20} />
            </div>
            <div>
              <h2 className="text-lg font-bold text-white">{currentRoom?.name}</h2>
              {currentRoom?.description && (
                <p className="text-xs text-slate-400">{currentRoom.description}</p>
              )}
            </div>
          </div>
        </header>

        {/* Messages */}
        <div 
          className="flex-1 overflow-y-auto p-6 space-y-6"
          ref={messageContainerRef}
          onScroll={handleScroll}
        >
          {isLoadingHistory && (
            <div className="flex justify-center py-2">
              <Loader2 className="w-5 h-5 animate-spin text-indigo-500" />
            </div>
          )}
          {currentMessages.length === 0 ? (
            <div className="h-full flex flex-col items-center justify-center text-slate-500 space-y-4">
              <div className="w-16 h-16 rounded-2xl bg-white/5 flex items-center justify-center">
                <MessageSquare size={32} className="text-slate-600" />
              </div>
              <p>这里还很安静，发条消息打破沉默吧！</p>
            </div>
          ) : (
            currentMessages.map((msg) => {
              const isMe = msg.sender === currentUser.username;
              return (
                <div key={msg.id} className={`flex flex-col ${isMe ? 'items-end' : 'items-start'} animate-in fade-in slide-in-from-bottom-2 duration-300`}>
                  <div className="flex items-baseline gap-2 mb-1.5 px-1">
                    <span className={`text-sm font-medium ${msg.sender === 'AI Assistant' ? 'text-emerald-400' : 'text-slate-300'}`}>
                      {msg.sender}
                    </span>
                    <span className="text-xs text-slate-500">
                      {new Date(msg.timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}
                    </span>
                  </div>
                  
                  <div className={`relative max-w-[85%] sm:max-w-[70%] rounded-2xl px-4 py-3 shadow-sm ${
                    isMe 
                      ? 'bg-indigo-600 text-white rounded-tr-sm shadow-indigo-900/20' 
                      : msg.sender === 'AI Assistant'
                        ? 'bg-emerald-500/10 text-emerald-100 rounded-tl-sm border border-emerald-500/20'
                        : 'bg-white/5 text-slate-200 rounded-tl-sm border border-white/5'
                  }`}>
                    {msg.type === 'text' && (
                      <div className="flex items-center gap-2">
                        {msg.isGenerating && <Loader2 size={14} className="animate-spin text-emerald-400" />}
                        <p className="whitespace-pre-wrap break-words leading-relaxed">{msg.content}</p>
                        {isMe && msg.status === 'sending' && (
                          <Loader2 size={14} className="animate-spin text-slate-400 ml-1 flex-shrink-0" />
                        )}
                        {isMe && msg.status === 'failed' && (
                          <div className="flex items-center ml-1 flex-shrink-0 gap-1">
                            <AlertCircle size={14} className="text-red-400" title="发送失败" />
                            <button 
                              onClick={() => sendClientMessage(currentRoom!.id, msg.content, msg.clientMessageId!, true)}
                              className="text-slate-400 hover:text-white transition-colors bg-white/5 hover:bg-white/10 rounded p-0.5"
                              title="重新发送"
                            >
                              <RefreshCw size={12} />
                            </button>
                          </div>
                        )}
                      </div>
                    )}
                    
                    {msg.type === 'image' && (
                      <div className="space-y-3">
                        <p className={`text-sm opacity-90 flex items-center gap-1.5 ${isMe ? 'text-indigo-100' : 'text-slate-400'}`}>
                          <Sparkles size={14} />
                          {msg.content}
                        </p>
                        {msg.isGenerating ? (
                          <div className="w-64 h-64 bg-black/20 rounded-xl flex flex-col items-center justify-center gap-3 animate-pulse border border-white/5">
                            <Loader2 className="w-8 h-8 animate-spin text-indigo-400" />
                            <span className="text-sm font-medium text-slate-400">AI 正在绘制...</span>
                          </div>
                        ) : msg.imageUrl ? (
                          <img 
                            src={msg.imageUrl} 
                            alt={msg.content} 
                            className="rounded-xl max-w-full h-auto shadow-lg hover:shadow-xl transition-shadow cursor-pointer border border-white/10"
                            referrerPolicy="no-referrer"
                            onClick={() => window.open(msg.imageUrl, '_blank')}
                          />
                        ) : null}
                      </div>
                    )}
                  </div>
                </div>
              );
            })
          )}
          <div ref={messagesEndRef} />
        </div>

        {/* Unread Messages Badge */}
        {unreadCount > 0 && (
          <button
            onClick={() => {
              scrollToBottom();
              setUnreadCount(0);
            }}
            className="absolute bottom-28 right-8 bg-indigo-600 text-white px-4 py-2 rounded-full shadow-lg flex items-center gap-2 hover:bg-indigo-500 transition-all animate-in slide-in-from-bottom-5 z-20 border border-indigo-400/30"
          >
            <span className="flex h-2 w-2 relative">
              <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-white opacity-75"></span>
              <span className="relative inline-flex rounded-full h-2 w-2 bg-white"></span>
            </span>
            <span className="text-sm font-medium">{unreadCount} 条新消息</span>
          </button>
        )}

        {/* Input Area */}
        <div className="p-4 sm:p-6 bg-[#131825] border-t border-white/5">
          <form onSubmit={handleSendMessage} className="flex items-end gap-3 max-w-5xl mx-auto">
            <div className="flex-1 relative group">
              <input
                type="text"
                value={inputValue}
                onChange={(e) => setInputValue(e.target.value)}
                placeholder={currentRoom ? `在 #${currentRoom.name} 中发送消息...` : '请选择一个频道...'}
                className="w-full bg-black/20 border border-white/10 text-white rounded-2xl pl-4 pr-12 py-3.5 focus:outline-none focus:ring-2 focus:ring-indigo-500/50 focus:border-indigo-500/50 focus:bg-black/40 hover:bg-black/30 transition-all shadow-inner placeholder:text-slate-500"
                disabled={isGenerating || !currentRoom}
              />
            </div>
            
            <button
              type="button"
              onClick={handleGenerateImage}
              disabled={isGenerating || !inputValue.trim() || !currentRoom}
              title="使用 AI 生成图片"
              className="p-3.5 text-indigo-400 bg-indigo-500/10 hover:bg-indigo-500/20 border border-indigo-500/20 rounded-2xl transition-colors disabled:opacity-50 disabled:cursor-not-allowed flex-shrink-0"
            >
              <ImageIcon size={22} />
            </button>
            
            <button
              type="submit"
              disabled={isGenerating || !inputValue.trim() || !currentRoom}
              className="p-3.5 bg-indigo-600 hover:bg-indigo-500 text-white rounded-2xl transition-all disabled:opacity-50 disabled:cursor-not-allowed flex-shrink-0 shadow-lg shadow-indigo-900/20 transform hover:-translate-y-0.5 border border-indigo-500/50"
            >
              <Send size={22} />
            </button>
          </form>
        </div>
      </main>

      {/* Create Room Modal */}
      {showCreateRoom && (
        <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/60 backdrop-blur-sm animate-in fade-in duration-200">
          <div className="bg-[#131825] border border-white/10 rounded-3xl p-6 w-full max-w-md shadow-2xl animate-in zoom-in-95 duration-200">
            <div className="flex items-center justify-between mb-6">
              <h3 className="text-xl font-bold text-white flex items-center gap-2">
                <Hash className="text-indigo-400" />
                创建新频道
              </h3>
              <button 
                onClick={() => setShowCreateRoom(false)}
                className="text-slate-400 hover:text-white p-1 rounded-lg hover:bg-white/5 transition-colors"
              >
                <X size={20} />
              </button>
            </div>
            
            <form onSubmit={handleCreateRoom} className="space-y-4">
              <div className="space-y-1.5">
                <label className="block text-sm font-medium text-slate-300 ml-1">频道名称</label>
                <input
                  type="text"
                  required
                  value={newRoomName}
                  onChange={(e) => setNewRoomName(e.target.value)}
                  className="block w-full px-4 py-3 border border-white/10 rounded-xl bg-black/20 focus:bg-black/40 focus:ring-2 focus:ring-indigo-500/50 focus:border-indigo-500/50 transition-all text-white placeholder:text-slate-600 outline-none"
                  placeholder="例如：游戏开黑"
                />
              </div>
              <div className="space-y-1.5">
                <label className="block text-sm font-medium text-slate-300 ml-1">频道描述 (可选)</label>
                <input
                  type="text"
                  value={newRoomDesc}
                  onChange={(e) => setNewRoomDesc(e.target.value)}
                  className="block w-full px-4 py-3 border border-white/10 rounded-xl bg-black/20 focus:bg-black/40 focus:ring-2 focus:ring-indigo-500/50 focus:border-indigo-500/50 transition-all text-white placeholder:text-slate-600 outline-none"
                  placeholder="这个频道是用来做什么的？"
                />
              </div>
              <div className="pt-4 flex gap-3">
                <button
                  type="button"
                  onClick={() => setShowCreateRoom(false)}
                  className="flex-1 py-3 px-4 bg-white/5 hover:bg-white/10 text-white rounded-xl font-medium transition-colors"
                >
                  取消
                </button>
                <button
                  type="submit"
                  className="flex-1 py-3 px-4 bg-indigo-600 hover:bg-indigo-500 text-white rounded-xl font-medium shadow-lg shadow-indigo-900/20 transition-colors border border-indigo-500/50"
                >
                  创建频道
                </button>
              </div>
            </form>
          </div>
        </div>
      )}
    </div>
  );
}
