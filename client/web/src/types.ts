export interface Message {
  id: string;
  sender: string;
  content: string;
  timestamp: Date;
  type: 'text' | 'image';
  imageUrl?: string;
  isGenerating?: boolean;
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
