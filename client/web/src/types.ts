export interface Message {
  id: string;
  clientMessageId?: string;
  serverMessageId?: number;
  sender: string;
  content: string;
  timestamp: Date;
  type: 'text' | 'image';
  imageUrl?: string;
  isGenerating?: boolean;
  status?: 'sending' | 'success' | 'failed';
}

export interface User {
  userid?: number;
  username: string;
  email?: string;
}

export interface Room {
  id: string;
  name: string;
  description?: string;
}
