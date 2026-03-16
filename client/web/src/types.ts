export interface Message {
  id: string;
  clientMessageId?: string;
  serverMessageId?: number; // 后端生成的全局唯一递增 ID
  sender: string;
  content: string;
  timestamp: Date;
  type: 'text' | 'image';
  imageUrl?: string;
  isGenerating?: boolean;
  status?: 'sending' | 'success' | 'failed'; // 客户端发送状态
}

export interface User {
  username: string;
  email: string;
}

export interface Room {
  id: string;
  name: string;
  description?: string;
}
