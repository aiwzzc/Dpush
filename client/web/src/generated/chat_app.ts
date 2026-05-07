import * as flatbuffers from 'flatbuffers';

export enum MsgContentType { Unknown = 0, Text = 1, Image = 2, Audio = 3 }
export enum ChatType { Single = 1, Group = 2 }
export enum AnyPayload { NONE = 0, HelloMessagePayload = 1, ClientMessagePayload = 2, ServerMessagePayload = 3, RequestRoomHistoryPayload = 4, RequestMessagePayload = 5, PullMissingMessagePayload = 6, MessageAckPayload = 7, BatchPullMessagePayload = 8, JoinSessionPayload = 9, SignalingFromClientPayload = 10, SignalingFromServerPayload = 11, SignalingFromClientJoinPayload = 12, PingPayload = 13, PongPayload = 14, BatchPullRoomHistoryPayload = 15, createSessionHttpReqbodyPayload = 16, createSessionHttpResbodyPayload = 17, LoginHttpReqbodyPayload = 18, LoginHttpResbodyPayload = 19, RegisterHttpReqbodyPayload = 20, RegisterHttpResbodyPayload = 21, JoinSessionHttpReqbodyPayload = 22, JoinSessionHttpResbodyPayload = 23 }

export class BatchPullMessageItem {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): BatchPullMessageItem {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  roomId(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  lastMessageId(): bigint {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.readInt64(this.bb_pos + offset) : BigInt(0);
  }
  static startBatchPullMessageItem(builder: flatbuffers.Builder) {
    builder.startObject(2);
  }
  static addRoomId(builder: flatbuffers.Builder, roomIdOffset: number) {
    builder.addFieldOffset(0, roomIdOffset, 0);
  }
  static addLastMessageId(builder: flatbuffers.Builder, lastMessageId: bigint) {
    builder.addFieldInt64(1, lastMessageId, BigInt(0));
  }
  static endBatchPullMessageItem(builder: flatbuffers.Builder): number {
    return builder.endObject();
  }
}

export class BatchPullMessagePayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): BatchPullMessagePayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  rooms(index: number, obj?: BatchPullMessageItem): BatchPullMessageItem | null {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? (obj || new BatchPullMessageItem()).__init(this.bb!.__indirect(this.bb!.__vector(this.bb_pos + offset) + index * 4), this.bb!) : null;
  }
  roomsLength(): number {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.__vector_len(this.bb_pos + offset) : 0;
  }
  static startBatchPullMessagePayload(builder: flatbuffers.Builder) {
    builder.startObject(1);
  }
  static addRooms(builder: flatbuffers.Builder, roomsOffset: number) {
    builder.addFieldOffset(0, roomsOffset, 0);
  }
  static createRoomsVector(builder: flatbuffers.Builder, data: number[] | Uint8Array): number {
    builder.startVector(4, data.length, 4);
    for (let i = data.length - 1; i >= 0; i--) {
      builder.addOffset(data[i]!);
    }
    return builder.endVector();
  }
  static endBatchPullMessagePayload(builder: flatbuffers.Builder): number {
    return builder.endObject();
  }
}

export class User {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): User {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  userid(): bigint {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.readInt64(this.bb_pos + offset) : BigInt(0);
  }
  username(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
}

export class RoomMessageItem {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): RoomMessageItem {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  id(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  content(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  user(obj?: User): User | null {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? (obj || new User()).__init(this.bb!.__indirect(this.bb_pos + offset), this.bb!) : null;
  }
  timestamp(): bigint {
    const offset = this.bb!.__offset(this.bb_pos, 10);
    return offset ? this.bb!.readInt64(this.bb_pos + offset) : BigInt(0);
  }
  msgType(): MsgContentType {
    const offset = this.bb!.__offset(this.bb_pos, 12);
    return offset ? this.bb!.readInt8(this.bb_pos + offset) : MsgContentType.Text;
  }
  imageUrl(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 14);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  replyTo(): bigint {
    const offset = this.bb!.__offset(this.bb_pos, 16);
    return offset ? this.bb!.readInt64(this.bb_pos + offset) : BigInt(0);
  }
}

export class RoomItem {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): RoomItem {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  hasMoreMessages(): boolean {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? !!this.bb!.readInt8(this.bb_pos + offset) : false;
  }
  roomId(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  roomname(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  messages(index: number, obj?: RoomMessageItem): RoomMessageItem | null {
    const offset = this.bb!.__offset(this.bb_pos, 10);
    return offset ? (obj || new RoomMessageItem()).__init(this.bb!.__indirect(this.bb!.__vector(this.bb_pos + offset) + index * 4), this.bb!) : null;
  }
  messagesLength(): number {
    const offset = this.bb!.__offset(this.bb_pos, 10);
    return offset ? this.bb!.__vector_len(this.bb_pos + offset) : 0;
  }
}

export class ClientMessageItem {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): ClientMessageItem {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  content(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  msgType(): MsgContentType {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.readInt8(this.bb_pos + offset) : MsgContentType.Text;
  }
  imageUrl(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  replyTo(): bigint {
    const offset = this.bb!.__offset(this.bb_pos, 10);
    return offset ? this.bb!.readInt64(this.bb_pos + offset) : BigInt(0);
  }

  static startClientMessageItem(builder: flatbuffers.Builder) {
    builder.startObject(4);
  }
  static addContent(builder: flatbuffers.Builder, contentOffset: number) {
    builder.addFieldOffset(0, contentOffset, 0);
  }
  static addMsgType(builder: flatbuffers.Builder, msgType: MsgContentType) {
    builder.addFieldInt8(1, msgType, MsgContentType.Text);
  }
  static addImageUrl(builder: flatbuffers.Builder, imageUrlOffset: number) {
    builder.addFieldOffset(2, imageUrlOffset, 0);
  }
  static addReplyTo(builder: flatbuffers.Builder, replyTo: bigint) {
    builder.addFieldInt64(3, replyTo, BigInt(0));
  }
  static endClientMessageItem(builder: flatbuffers.Builder): number {
    return builder.endObject();
  }
}

export class ServerMessageItem {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): ServerMessageItem {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  clientMessageId(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  serverMessageId(): bigint {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.readInt64(this.bb_pos + offset) : BigInt(0);
  }
  content(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  user(obj?: User): User | null {
    const offset = this.bb!.__offset(this.bb_pos, 10);
    return offset ? (obj || new User()).__init(this.bb!.__indirect(this.bb_pos + offset), this.bb!) : null;
  }
  timestamp(): bigint {
    const offset = this.bb!.__offset(this.bb_pos, 12);
    return offset ? this.bb!.readInt64(this.bb_pos + offset) : BigInt(0);
  }
  msgType(): MsgContentType {
    const offset = this.bb!.__offset(this.bb_pos, 14);
    return offset ? this.bb!.readInt8(this.bb_pos + offset) : MsgContentType.Text;
  }
  imageUrl(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 16);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  replyTo(): bigint {
    const offset = this.bb!.__offset(this.bb_pos, 18);
    return offset ? this.bb!.readInt64(this.bb_pos + offset) : BigInt(0);
  }
}

export class RequestMessageItem {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): RequestMessageItem {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  id(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  content(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  user(obj?: User): User | null {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? (obj || new User()).__init(this.bb!.__indirect(this.bb_pos + offset), this.bb!) : null;
  }
  timestamp(): bigint {
    const offset = this.bb!.__offset(this.bb_pos, 10);
    return offset ? this.bb!.readInt64(this.bb_pos + offset) : BigInt(0);
  }
  msgType(): MsgContentType {
    const offset = this.bb!.__offset(this.bb_pos, 12);
    return offset ? this.bb!.readInt8(this.bb_pos + offset) : MsgContentType.Text;
  }
  imageUrl(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 14);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  replyTo(): bigint {
    const offset = this.bb!.__offset(this.bb_pos, 16);
    return offset ? this.bb!.readInt64(this.bb_pos + offset) : BigInt(0);
  }
}

export class ClientMessagePayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): ClientMessagePayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  chatType(): ChatType {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.readInt8(this.bb_pos + offset) : ChatType.Group;
  }
  targetId(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  clientMessageId(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  messages(obj?: ClientMessageItem): ClientMessageItem | null {
    const offset = this.bb!.__offset(this.bb_pos, 10);
    return offset ? (obj || new ClientMessageItem()).__init(this.bb!.__indirect(this.bb_pos + offset), this.bb!) : null;
  }

  static startClientMessagePayload(builder: flatbuffers.Builder) {
    builder.startObject(4);
  }
  static addChatType(builder: flatbuffers.Builder, chatType: ChatType) {
    builder.addFieldInt8(0, chatType, ChatType.Group);
  }
  static addTargetId(builder: flatbuffers.Builder, targetIdOffset: number) {
    builder.addFieldOffset(1, targetIdOffset, 0);
  }
  static addClientMessageId(builder: flatbuffers.Builder, clientMessageIdOffset: number) {
    builder.addFieldOffset(2, clientMessageIdOffset, 0);
  }
  static addMessages(builder: flatbuffers.Builder, messagesOffset: number) {
    builder.addFieldOffset(3, messagesOffset, 0);
  }
  static endClientMessagePayload(builder: flatbuffers.Builder): number {
    return builder.endObject();
  }
}

export class ServerMessagePayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): ServerMessagePayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  chatType(): ChatType {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.readInt8(this.bb_pos + offset) : ChatType.Group;
  }
  sessionId(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  messages(obj?: ServerMessageItem): ServerMessageItem | null {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? (obj || new ServerMessageItem()).__init(this.bb!.__indirect(this.bb_pos + offset), this.bb!) : null;
  }
}

export class PullMissingMessagePayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): PullMissingMessagePayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  static startPullMissingMessagePayload(builder: flatbuffers.Builder) {
    builder.startObject(2);
  }
  static addRoomId(builder: flatbuffers.Builder, roomIdOffset: number) {
    builder.addFieldOffset(0, roomIdOffset, 0);
  }
  static addMissingMessageIds(builder: flatbuffers.Builder, missingMessageIdsOffset: number) {
    builder.addFieldOffset(1, missingMessageIdsOffset, 0);
  }
  static createMissingMessageIdsVector(builder: flatbuffers.Builder, data: bigint[] | number[]): number {
    builder.startVector(8, data.length, 8);
    for (let i = data.length - 1; i >= 0; i--) {
      builder.addInt64(BigInt(data[i]));
    }
    return builder.endVector();
  }
  static endPullMissingMessagePayload(builder: flatbuffers.Builder): number {
    return builder.endObject();
  }
}

export class RequestRoomHistoryPayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): RequestRoomHistoryPayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  static startRequestRoomHistoryPayload(builder: flatbuffers.Builder) {
    builder.startObject(3);
  }
  static addRoomId(builder: flatbuffers.Builder, roomIdOffset: number) {
    builder.addFieldOffset(0, roomIdOffset, 0);
  }
  static addCount(builder: flatbuffers.Builder, count: bigint) {
    builder.addFieldInt64(1, count, BigInt(0));
  }
  static addFirstMessageId(builder: flatbuffers.Builder, firstMessageIdOffset: number) {
    builder.addFieldOffset(2, firstMessageIdOffset, 0);
  }
  static endRequestRoomHistoryPayload(builder: flatbuffers.Builder): number {
    return builder.endObject();
  }
}

export class RequestMessagePayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): RequestMessagePayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  roomId(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  roomname(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  hasMoreMessages(): boolean {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? !!this.bb!.readInt8(this.bb_pos + offset) : false;
  }
  messagesLength(): number {
    const offset = this.bb!.__offset(this.bb_pos, 10);
    return offset ? this.bb!.__vector_len(this.bb_pos + offset) : 0;
  }
  messages(index: number, obj?: RequestMessageItem): RequestMessageItem | null {
    const offset = this.bb!.__offset(this.bb_pos, 10);
    return offset ? (obj || new RequestMessageItem()).__init(this.bb!.__indirect(this.bb!.__vector(this.bb_pos + offset) + index * 4), this.bb!) : null;
  }
}

export class MessageAckPayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): MessageAckPayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  clientMessageId(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  status(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
}

export class JoinSessionPayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): JoinSessionPayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  roomId(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  roomname(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  messages(index: number, obj?: ServerMessageItem): ServerMessageItem | null {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? (obj || new ServerMessageItem()).__init(this.bb!.__indirect(this.bb!.__vector(this.bb_pos + offset) + index * 4), this.bb!) : null;
  }
  messagesLength(): number {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? this.bb!.__vector_len(this.bb_pos + offset) : 0;
  }

  static getRootAsJoinSessionPayload(bb: flatbuffers.ByteBuffer, obj?: JoinSessionPayload): JoinSessionPayload {
    return (obj || new JoinSessionPayload()).__init(bb.readInt32(bb.position()) + bb.position(), bb);
  }
}

export class RootMessage {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): RootMessage {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  static getRootAsRootMessage(bb: flatbuffers.ByteBuffer, obj?: RootMessage): RootMessage {
    return (obj || new RootMessage()).__init(bb.readInt32(bb.position()) + bb.position(), bb);
  }
  payloadType(): AnyPayload {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.readUint8(this.bb_pos + offset) : AnyPayload.NONE;
  }
  payload(obj: any): any {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? obj.__init(this.bb!.__indirect(this.bb_pos + offset), this.bb!) : null;
  }

  static startRootMessage(builder: flatbuffers.Builder) {
    builder.startObject(2);
  }
  static addPayloadType(builder: flatbuffers.Builder, payloadType: AnyPayload) {
    builder.addFieldInt8(0, payloadType, AnyPayload.NONE);
  }
  static addPayload(builder: flatbuffers.Builder, payloadOffset: number) {
    builder.addFieldOffset(1, payloadOffset, 0);
  }
  static endRootMessage(builder: flatbuffers.Builder): number {
    return builder.endObject();
  }
  static finishRootMessageBuffer(builder: flatbuffers.Builder, offset: number) {
    builder.finish(offset);
  }
}

export class SignalingFromClientPayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): SignalingFromClientPayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  action(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  roomId(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  static startSignalingFromClientPayload(builder: flatbuffers.Builder) {
    builder.startObject(2);
  }
  static addAction(builder: flatbuffers.Builder, actionOffset: number) {
    builder.addFieldOffset(0, actionOffset, 0);
  }
  static addRoomId(builder: flatbuffers.Builder, roomIdOffset: number) {
    builder.addFieldOffset(1, roomIdOffset, 0);
  }
  static endSignalingFromClientPayload(builder: flatbuffers.Builder): number {
    return builder.endObject();
  }
  static createSignalingFromClientPayload(builder: flatbuffers.Builder, actionOffset: number, roomIdOffset: number): number {
    SignalingFromClientPayload.startSignalingFromClientPayload(builder);
    SignalingFromClientPayload.addAction(builder, actionOffset);
    SignalingFromClientPayload.addRoomId(builder, roomIdOffset);
    return SignalingFromClientPayload.endSignalingFromClientPayload(builder);
  }
}

export class SignalingFromServerPayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): SignalingFromServerPayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  action(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  roomId(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  status(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
}

export class SignalingFromClientJoinPayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): SignalingFromClientJoinPayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  action(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  roomId(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  roomName(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  static startSignalingFromClientJoinPayload(builder: flatbuffers.Builder) {
    builder.startObject(3);
  }
  static addAction(builder: flatbuffers.Builder, actionOffset: number) {
    builder.addFieldOffset(0, actionOffset, 0);
  }
  static addRoomId(builder: flatbuffers.Builder, roomIdOffset: number) {
    builder.addFieldOffset(1, roomIdOffset, 0);
  }
  static addRoomName(builder: flatbuffers.Builder, roomNameOffset: number) {
    builder.addFieldOffset(2, roomNameOffset, 0);
  }
  static endSignalingFromClientJoinPayload(builder: flatbuffers.Builder): number {
    return builder.endObject();
  }
  static createSignalingFromClientJoinPayload(builder: flatbuffers.Builder, actionOffset: number, roomIdOffset: number, roomNameOffset: number): number {
    SignalingFromClientJoinPayload.startSignalingFromClientJoinPayload(builder);
    SignalingFromClientJoinPayload.addAction(builder, actionOffset);
    SignalingFromClientJoinPayload.addRoomId(builder, roomIdOffset);
    SignalingFromClientJoinPayload.addRoomName(builder, roomNameOffset);
    return SignalingFromClientJoinPayload.endSignalingFromClientJoinPayload(builder);
  }
}

export class PingPayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): PingPayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  ts(): bigint {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.readUint64(this.bb_pos + offset) : BigInt(0);
  }
  static startPingPayload(builder: flatbuffers.Builder) {
    builder.startObject(1);
  }
  static addTs(builder: flatbuffers.Builder, ts: bigint) {
    builder.addFieldInt64(0, ts, BigInt(0));
  }
  static endPingPayload(builder: flatbuffers.Builder): number {
    return builder.endObject();
  }
  static createPingPayload(builder: flatbuffers.Builder, ts: bigint): number {
    PingPayload.startPingPayload(builder);
    PingPayload.addTs(builder, ts);
    return PingPayload.endPingPayload(builder);
  }
}

export class PongPayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): PongPayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  ts(): bigint {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.readUint64(this.bb_pos + offset) : BigInt(0);
  }
  static startPongPayload(builder: flatbuffers.Builder) {
    builder.startObject(1);
  }
  static addTs(builder: flatbuffers.Builder, ts: bigint) {
    builder.addFieldInt64(0, ts, BigInt(0));
  }
  static endPongPayload(builder: flatbuffers.Builder): number {
    return builder.endObject();
  }
  static createPongPayload(builder: flatbuffers.Builder, ts: bigint): number {
    PongPayload.startPongPayload(builder);
    PongPayload.addTs(builder, ts);
    return PongPayload.endPongPayload(builder);
  }
}

export class ByteChunk {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): ByteChunk {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  data(index: number): number | null {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.readUint8(this.bb!.__vector(this.bb_pos + offset) + index) : 0;
  }
  dataLength(): number {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.__vector_len(this.bb_pos + offset) : 0;
  }
  dataArray(): Uint8Array | null {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? new Uint8Array(this.bb!.bytes().buffer, this.bb!.bytes().byteOffset + this.bb!.__vector(this.bb_pos + offset), this.bb!.__vector_len(this.bb_pos + offset)) : null;
  }
}

export class BatchPullRoomHistoryPayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): BatchPullRoomHistoryPayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }
  roomId(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.__string(this.bb_pos + offset) as string : null;
  }
  historyChunks(index: number, obj?: ByteChunk): ByteChunk | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? (obj || new ByteChunk()).__init(this.bb!.__indirect(this.bb!.__vector(this.bb_pos + offset) + index * 4), this.bb!) : null;
  }
  historyChunksLength(): number {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__vector_len(this.bb_pos + offset) : 0;
  }
}

export class createSessionHttpReqbodyPayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): createSessionHttpReqbodyPayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }

  static getRootAscreateSessionHttpReqbodyPayload(bb: flatbuffers.ByteBuffer, obj?: createSessionHttpReqbodyPayload): createSessionHttpReqbodyPayload {
    return (obj || new createSessionHttpReqbodyPayload()).__init(bb.readInt32(bb.position()) + bb.position(), bb);
  }

  userid(): bigint {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.readInt64(this.bb_pos + offset) : BigInt('0');
  }

  roomName(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }

  static startcreateSessionHttpReqbodyPayload(builder: flatbuffers.Builder) {
    builder.startObject(2);
  }

  static addUserid(builder: flatbuffers.Builder, userid: bigint) {
    builder.addFieldInt64(0, userid, BigInt('0'));
  }

  static addRoomName(builder: flatbuffers.Builder, roomNameOffset: number) {
    builder.addFieldOffset(1, roomNameOffset, 0);
  }

  static endcreateSessionHttpReqbodyPayload(builder: flatbuffers.Builder): number {
    const offset = builder.endObject();
    return offset;
  }

  static createcreateSessionHttpReqbodyPayload(builder: flatbuffers.Builder, userid: bigint, roomNameOffset: number): number {
    createSessionHttpReqbodyPayload.startcreateSessionHttpReqbodyPayload(builder);
    createSessionHttpReqbodyPayload.addUserid(builder, userid);
    createSessionHttpReqbodyPayload.addRoomName(builder, roomNameOffset);
    return createSessionHttpReqbodyPayload.endcreateSessionHttpReqbodyPayload(builder);
  }
}

export class createSessionHttpResbodyPayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): createSessionHttpResbodyPayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }

  static getRootAscreateSessionHttpResbodyPayload(bb: flatbuffers.ByteBuffer, obj?: createSessionHttpResbodyPayload): createSessionHttpResbodyPayload {
    return (obj || new createSessionHttpResbodyPayload()).__init(bb.readInt32(bb.position()) + bb.position(), bb);
  }

  code(): number {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.readInt32(this.bb_pos + offset) : 0;
  }

  errMsg(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }

  userid(): bigint {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? this.bb!.readInt64(this.bb_pos + offset) : BigInt('0');
  }

  roomId(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 10);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }

  static startcreateSessionHttpResbodyPayload(builder: flatbuffers.Builder) {
    builder.startObject(4);
  }

  static addCode(builder: flatbuffers.Builder, code: number) {
    builder.addFieldInt32(0, code, 0);
  }

  static addErrMsg(builder: flatbuffers.Builder, errMsgOffset: number) {
    builder.addFieldOffset(1, errMsgOffset, 0);
  }

  static addUserid(builder: flatbuffers.Builder, userid: bigint) {
    builder.addFieldInt64(2, userid, BigInt('0'));
  }

  static addRoomId(builder: flatbuffers.Builder, roomIdOffset: number) {
    builder.addFieldOffset(3, roomIdOffset, 0);
  }

  static endcreateSessionHttpResbodyPayload(builder: flatbuffers.Builder): number {
    const offset = builder.endObject();
    return offset;
  }
}

export class LoginHttpReqbodyPayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): LoginHttpReqbodyPayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }

  static getRootAsLoginHttpReqbodyPayload(bb: flatbuffers.ByteBuffer, obj?: LoginHttpReqbodyPayload): LoginHttpReqbodyPayload {
    return (obj || new LoginHttpReqbodyPayload()).__init(bb.readInt32(bb.position()) + bb.position(), bb);
  }

  email(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }

  password(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }

  static startLoginHttpReqbodyPayload(builder: flatbuffers.Builder) {
    builder.startObject(2);
  }

  static addEmail(builder: flatbuffers.Builder, emailOffset: number) {
    builder.addFieldOffset(0, emailOffset, 0);
  }

  static addPassword(builder: flatbuffers.Builder, passwordOffset: number) {
    builder.addFieldOffset(1, passwordOffset, 0);
  }

  static endLoginHttpReqbodyPayload(builder: flatbuffers.Builder): number {
    const offset = builder.endObject();
    return offset;
  }

  static createLoginHttpReqbodyPayload(builder: flatbuffers.Builder, emailOffset: number, passwordOffset: number): number {
    LoginHttpReqbodyPayload.startLoginHttpReqbodyPayload(builder);
    LoginHttpReqbodyPayload.addEmail(builder, emailOffset);
    LoginHttpReqbodyPayload.addPassword(builder, passwordOffset);
    return LoginHttpReqbodyPayload.endLoginHttpReqbodyPayload(builder);
  }
}

export class LoginHttpResbodyPayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): LoginHttpResbodyPayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }

  static getRootAsLoginHttpResbodyPayload(bb: flatbuffers.ByteBuffer, obj?: LoginHttpResbodyPayload): LoginHttpResbodyPayload {
    return (obj || new LoginHttpResbodyPayload()).__init(bb.readInt32(bb.position()) + bb.position(), bb);
  }

  code(): number {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.readInt32(this.bb_pos + offset) : 0;
  }

  errMsg(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }

  userid(): bigint {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? this.bb!.readInt64(this.bb_pos + offset) : BigInt('0');
  }

  username(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 10);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }

  token(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 12);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }

  static startLoginHttpResbodyPayload(builder: flatbuffers.Builder) {
    builder.startObject(5);
  }

  static addCode(builder: flatbuffers.Builder, code: number) {
    builder.addFieldInt32(0, code, 0);
  }

  static addErrMsg(builder: flatbuffers.Builder, errMsgOffset: number) {
    builder.addFieldOffset(1, errMsgOffset, 0);
  }

  static addUserid(builder: flatbuffers.Builder, userid: bigint) {
    builder.addFieldInt64(2, userid, BigInt('0'));
  }

  static addUsername(builder: flatbuffers.Builder, usernameOffset: number) {
    builder.addFieldOffset(3, usernameOffset, 0);
  }

  static addToken(builder: flatbuffers.Builder, tokenOffset: number) {
    builder.addFieldOffset(4, tokenOffset, 0);
  }

  static endLoginHttpResbodyPayload(builder: flatbuffers.Builder): number {
    const offset = builder.endObject();
    return offset;
  }
}

export class RegisterHttpReqbodyPayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): RegisterHttpReqbodyPayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }

  static getRootAsRegisterHttpReqbodyPayload(bb: flatbuffers.ByteBuffer, obj?: RegisterHttpReqbodyPayload): RegisterHttpReqbodyPayload {
    return (obj || new RegisterHttpReqbodyPayload()).__init(bb.readInt32(bb.position()) + bb.position(), bb);
  }

  username(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }

  email(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }

  password(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }

  static startRegisterHttpReqbodyPayload(builder: flatbuffers.Builder) {
    builder.startObject(3);
  }

  static addUsername(builder: flatbuffers.Builder, usernameOffset: number) {
    builder.addFieldOffset(0, usernameOffset, 0);
  }

  static addEmail(builder: flatbuffers.Builder, emailOffset: number) {
    builder.addFieldOffset(1, emailOffset, 0);
  }

  static addPassword(builder: flatbuffers.Builder, passwordOffset: number) {
    builder.addFieldOffset(2, passwordOffset, 0);
  }

  static endRegisterHttpReqbodyPayload(builder: flatbuffers.Builder): number {
    const offset = builder.endObject();
    return offset;
  }

  static createRegisterHttpReqbodyPayload(builder: flatbuffers.Builder, usernameOffset: number, emailOffset: number, passwordOffset: number): number {
    RegisterHttpReqbodyPayload.startRegisterHttpReqbodyPayload(builder);
    RegisterHttpReqbodyPayload.addUsername(builder, usernameOffset);
    RegisterHttpReqbodyPayload.addEmail(builder, emailOffset);
    RegisterHttpReqbodyPayload.addPassword(builder, passwordOffset);
    return RegisterHttpReqbodyPayload.endRegisterHttpReqbodyPayload(builder);
  }
}

export class RegisterHttpResbodyPayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): RegisterHttpResbodyPayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }

  static getRootAsRegisterHttpResbodyPayload(bb: flatbuffers.ByteBuffer, obj?: RegisterHttpResbodyPayload): RegisterHttpResbodyPayload {
    return (obj || new RegisterHttpResbodyPayload()).__init(bb.readInt32(bb.position()) + bb.position(), bb);
  }

  code(): number {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.readInt32(this.bb_pos + offset) : 0;
  }

  errMsg(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }

  static startRegisterHttpResbodyPayload(builder: flatbuffers.Builder) {
    builder.startObject(2);
  }

  static addCode(builder: flatbuffers.Builder, code: number) {
    builder.addFieldInt32(0, code, 0);
  }

  static addErrMsg(builder: flatbuffers.Builder, errMsgOffset: number) {
    builder.addFieldOffset(1, errMsgOffset, 0);
  }

  static endRegisterHttpResbodyPayload(builder: flatbuffers.Builder): number {
    const offset = builder.endObject();
    return offset;
  }
}

export class JoinSessionHttpReqbodyPayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): JoinSessionHttpReqbodyPayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }

  static getRootAsJoinSessionHttpReqbodyPayload(bb: flatbuffers.ByteBuffer, obj?: JoinSessionHttpReqbodyPayload): JoinSessionHttpReqbodyPayload {
    return (obj || new JoinSessionHttpReqbodyPayload()).__init(bb.readInt32(bb.position()) + bb.position(), bb);
  }

  userid(): bigint {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.readInt64(this.bb_pos + offset) : BigInt('0');
  }

  roomName(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }

  static startJoinSessionHttpReqbodyPayload(builder: flatbuffers.Builder) {
    builder.startObject(2);
  }

  static addUserid(builder: flatbuffers.Builder, userid: bigint) {
    builder.addFieldInt64(0, userid, BigInt('0'));
  }

  static addRoomName(builder: flatbuffers.Builder, roomNameOffset: number) {
    builder.addFieldOffset(1, roomNameOffset, 0);
  }

  static endJoinSessionHttpReqbodyPayload(builder: flatbuffers.Builder): number {
    const offset = builder.endObject();
    return offset;
  }

  static createJoinSessionHttpReqbodyPayload(builder: flatbuffers.Builder, userid: bigint, roomNameOffset: number): number {
    JoinSessionHttpReqbodyPayload.startJoinSessionHttpReqbodyPayload(builder);
    JoinSessionHttpReqbodyPayload.addUserid(builder, userid);
    JoinSessionHttpReqbodyPayload.addRoomName(builder, roomNameOffset);
    return JoinSessionHttpReqbodyPayload.endJoinSessionHttpReqbodyPayload(builder);
  }
}

export class JoinSessionHttpResbodyPayload {
  bb: flatbuffers.ByteBuffer | null = null;
  bb_pos = 0;
  __init(i: number, bb: flatbuffers.ByteBuffer): JoinSessionHttpResbodyPayload {
    this.bb_pos = i;
    this.bb = bb;
    return this;
  }

  static getRootAsJoinSessionHttpResbodyPayload(bb: flatbuffers.ByteBuffer, obj?: JoinSessionHttpResbodyPayload): JoinSessionHttpResbodyPayload {
    return (obj || new JoinSessionHttpResbodyPayload()).__init(bb.readInt32(bb.position()) + bb.position(), bb);
  }

  code(): number {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.readInt32(this.bb_pos + offset) : 0;
  }

  errMsg(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }

  userid(): bigint {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? this.bb!.readInt64(this.bb_pos + offset) : BigInt('0');
  }

  roomId(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 10);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }

  static startJoinSessionHttpResbodyPayload(builder: flatbuffers.Builder) {
    builder.startObject(4);
  }

  static addCode(builder: flatbuffers.Builder, code: number) {
    builder.addFieldInt32(0, code, 0);
  }

  static addErrMsg(builder: flatbuffers.Builder, errMsgOffset: number) {
    builder.addFieldOffset(1, errMsgOffset, 0);
  }

  static addUserid(builder: flatbuffers.Builder, userid: bigint) {
    builder.addFieldInt64(2, userid, BigInt('0'));
  }

  static addRoomId(builder: flatbuffers.Builder, roomIdOffset: number) {
    builder.addFieldOffset(3, roomIdOffset, 0);
  }

  static endJoinSessionHttpResbodyPayload(builder: flatbuffers.Builder): number {
    const offset = builder.endObject();
    return offset;
  }
}
