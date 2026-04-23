import * as flatbuffers from 'flatbuffers';

export enum MsgContentType { Unknown = 0, Text = 1, Image = 2, Audio = 3 }
export enum AnyPayload { NONE = 0, HelloMessagePayload = 1, ClientMessagePayload = 2, ServerMessagePayload = 3, RequestRoomHistoryPayload = 4, RequestMessagePayload = 5, PullMissingMessagePayload = 6, MessageAckPayload = 7, BatchPullMessagePayload = 8, JoinSessionPayload = 9, SignalingFromClientPayload = 10, SignalingFromServerPayload = 11, SignalingFromClientJoinPayload = 12 }

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
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
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
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
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
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }
  content(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
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
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
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
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }
  roomname(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
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
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }
  msgType(): MsgContentType {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.readInt8(this.bb_pos + offset) : MsgContentType.Text;
  }
  imageUrl(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }

  static startClientMessageItem(builder: flatbuffers.Builder) {
    builder.startObject(3);
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
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }
  serverMessageId(): bigint {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.readInt64(this.bb_pos + offset) : BigInt(0);
  }
  content(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
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
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
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
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }
  content(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
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
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
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
  static startClientMessagePayload(builder: flatbuffers.Builder) {
    builder.startObject(3);
  }
  static addRoomId(builder: flatbuffers.Builder, roomIdOffset: number) {
    builder.addFieldOffset(0, roomIdOffset, 0);
  }
  static addClientMessageId(builder: flatbuffers.Builder, clientMessageIdOffset: number) {
    builder.addFieldOffset(1, clientMessageIdOffset, 0);
  }
  static addMessages(builder: flatbuffers.Builder, messagesOffset: number) {
    builder.addFieldOffset(2, messagesOffset, 0);
  }
  static createMessagesVector(builder: flatbuffers.Builder, data: number[]): number {
    builder.startVector(4, data.length, 4);
    for (let i = data.length - 1; i >= 0; i--) {
      builder.addOffset(data[i]);
    }
    return builder.endVector();
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
  roomId(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 4);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }
  messagesLength(): number {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__vector_len(this.bb_pos + offset) : 0;
  }
  messages(index: number, obj?: ServerMessageItem): ServerMessageItem | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? (obj || new ServerMessageItem()).__init(this.bb!.__indirect(this.bb!.__vector(this.bb_pos + offset) + index * 4), this.bb!) : null;
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
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }
  roomname(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
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
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }
  status(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
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
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }
  roomname(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
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
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }
  roomId(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
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
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }
  roomId(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }
  status(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
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
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }
  roomId(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 6);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
  }
  roomName(): string | null {
    const offset = this.bb!.__offset(this.bb_pos, 8);
    return offset ? this.bb!.__string(this.bb_pos + offset) : null;
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
