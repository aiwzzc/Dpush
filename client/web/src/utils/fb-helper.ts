import * as flatbuffers from 'flatbuffers';
import {
  RootMessage,
  AnyPayload,
  ClientMessagePayload,
  ClientMessageItem,
  MsgContentType,
  PullMissingMessagePayload,
  RequestRoomHistoryPayload,
  BatchPullMessagePayload,
  BatchPullMessageItem,
  SignalingFromClientPayload,
  SignalingFromClientJoinPayload
} from '../generated/chat_app';

export function encodeClientMessage(roomId: string, clientMessageId: string, content: string, msgType: 'text' | 'image' = 'text', imageUrl?: string): Uint8Array {
  const builder = new flatbuffers.Builder(1024);

  const roomIdOffset = builder.createString(roomId);
  const clientIdOffset = builder.createString(clientMessageId);
  const contentOffset = builder.createString(content);
  const imageUrlOffset = imageUrl ? builder.createString(imageUrl) : 0;

  ClientMessageItem.startClientMessageItem(builder);
  ClientMessageItem.addContent(builder, contentOffset);
  ClientMessageItem.addMsgType(builder, msgType === 'image' ? MsgContentType.Image : MsgContentType.Text);
  if (imageUrlOffset) {
    ClientMessageItem.addImageUrl(builder, imageUrlOffset);
  }
  const itemOffset = ClientMessageItem.endClientMessageItem(builder);

  const messagesOffset = ClientMessagePayload.createMessagesVector(builder, [itemOffset]);

  ClientMessagePayload.startClientMessagePayload(builder);
  ClientMessagePayload.addRoomId(builder, roomIdOffset);
  ClientMessagePayload.addClientMessageId(builder, clientIdOffset);
  ClientMessagePayload.addMessages(builder, messagesOffset);
  const payloadOffset = ClientMessagePayload.endClientMessagePayload(builder);

  RootMessage.startRootMessage(builder);
  RootMessage.addPayloadType(builder, AnyPayload.ClientMessagePayload);
  RootMessage.addPayload(builder, payloadOffset);
  const rootOffset = RootMessage.endRootMessage(builder);

  builder.finish(rootOffset);
  return builder.asUint8Array();
}

export function encodePullMissingMessages(roomId: string, missingIds: number[]): Uint8Array {
  const builder = new flatbuffers.Builder(1024);
  const roomIdOffset = builder.createString(roomId);

  // Convert number[] to bigint[] for flatbuffers long
  const bigIntIds = missingIds.map(id => BigInt(id));
  const idsOffset = PullMissingMessagePayload.createMissingMessageIdsVector(builder, bigIntIds);

  PullMissingMessagePayload.startPullMissingMessagePayload(builder);
  PullMissingMessagePayload.addRoomId(builder, roomIdOffset);
  PullMissingMessagePayload.addMissingMessageIds(builder, idsOffset);
  const payloadOffset = PullMissingMessagePayload.endPullMissingMessagePayload(builder);

  RootMessage.startRootMessage(builder);
  RootMessage.addPayloadType(builder, AnyPayload.PullMissingMessagePayload);
  RootMessage.addPayload(builder, payloadOffset);
  const rootOffset = RootMessage.endRootMessage(builder);

  builder.finish(rootOffset);
  return builder.asUint8Array();
}

export function encodeRequestRoomHistory(roomId: string, firstMessageId: string, count: number): Uint8Array {
  const builder = new flatbuffers.Builder(1024);
  const roomIdOffset = builder.createString(roomId);
  const firstMsgIdOffset = builder.createString(firstMessageId);

  RequestRoomHistoryPayload.startRequestRoomHistoryPayload(builder);
  RequestRoomHistoryPayload.addRoomId(builder, roomIdOffset);
  RequestRoomHistoryPayload.addFirstMessageId(builder, firstMsgIdOffset);
  RequestRoomHistoryPayload.addCount(builder, BigInt(count));
  const payloadOffset = RequestRoomHistoryPayload.endRequestRoomHistoryPayload(builder);

  RootMessage.startRootMessage(builder);
  RootMessage.addPayloadType(builder, AnyPayload.RequestRoomHistoryPayload);
  RootMessage.addPayload(builder, payloadOffset);
  const rootOffset = RootMessage.endRootMessage(builder);

  builder.finish(rootOffset);
  return builder.asUint8Array();
}

export function encodeBatchPullMessage(roomMaxIds: { roomId: string, maxId: number }[]): Uint8Array {
  const builder = new flatbuffers.Builder(1024);

  const itemOffsets = roomMaxIds.map(rm => {
    const roomIdOffset = builder.createString(rm.roomId);
    BatchPullMessageItem.startBatchPullMessageItem(builder);
    BatchPullMessageItem.addRoomId(builder, roomIdOffset);
    BatchPullMessageItem.addLastMessageId(builder, BigInt(rm.maxId));
    return BatchPullMessageItem.endBatchPullMessageItem(builder);
  });

  const roomsVectorOffset = BatchPullMessagePayload.createRoomsVector(builder, itemOffsets);

  BatchPullMessagePayload.startBatchPullMessagePayload(builder);
  BatchPullMessagePayload.addRooms(builder, roomsVectorOffset);
  const payloadOffset = BatchPullMessagePayload.endBatchPullMessagePayload(builder);

  RootMessage.startRootMessage(builder);
  RootMessage.addPayloadType(builder, AnyPayload.BatchPullMessagePayload);
  RootMessage.addPayload(builder, payloadOffset);
  const rootOffset = RootMessage.endRootMessage(builder);

  builder.finish(rootOffset);
  return builder.asUint8Array();
}

export function encodeSignalingFromClient(action: string, roomId: string): Uint8Array {
  const builder = new flatbuffers.Builder(1024);

  const actionOffset = builder.createString(action);
  const roomIdOffset = builder.createString(roomId);

  SignalingFromClientPayload.startSignalingFromClientPayload(builder);
  SignalingFromClientPayload.addAction(builder, actionOffset);
  SignalingFromClientPayload.addRoomId(builder, roomIdOffset);
  const payloadOffset = SignalingFromClientPayload.endSignalingFromClientPayload(builder);

  RootMessage.startRootMessage(builder);
  RootMessage.addPayloadType(builder, AnyPayload.SignalingFromClientPayload);
  RootMessage.addPayload(builder, payloadOffset);
  const rootOffset = RootMessage.endRootMessage(builder);

  builder.finish(rootOffset);
  return builder.asUint8Array();
}

export function encodeSignalingFromClientJoin(action: string, roomId: string, roomName: string): Uint8Array {
  const builder = new flatbuffers.Builder(1024);

  const actionOffset = builder.createString(action);
  const roomIdOffset = builder.createString(roomId);
  const roomNameOffset = builder.createString(roomName);

  SignalingFromClientJoinPayload.startSignalingFromClientJoinPayload(builder);
  SignalingFromClientJoinPayload.addAction(builder, actionOffset);
  SignalingFromClientJoinPayload.addRoomId(builder, roomIdOffset);
  SignalingFromClientJoinPayload.addRoomName(builder, roomNameOffset);
  const payloadOffset = SignalingFromClientJoinPayload.endSignalingFromClientJoinPayload(builder);

  RootMessage.startRootMessage(builder);
  RootMessage.addPayloadType(builder, AnyPayload.SignalingFromClientJoinPayload);
  RootMessage.addPayload(builder, payloadOffset);
  const rootOffset = RootMessage.endRootMessage(builder);

  builder.finish(rootOffset);
  return builder.asUint8Array();
}
