export enum MsgContentType {
  Unknown = 0,
  Text = 1,
  Image = 2,
  Audio = 3
}

export enum ChatType {
  Single = 1,
  Group = 2
}

export enum AnyPayload {
  NONE = 0,
  HelloMessagePayload = 1,
  ClientMessagePayload = 2,
  ServerMessagePayload = 3,
  RequestRoomHistoryPayload = 4,
  RequestMessagePayload = 5,
  PullMissingMessagePayload = 6,
  MessageAckPayload = 7,
  BatchPullMessagePayload = 8,
  JoinSessionPayload = 9,
  SignalingFromClientPayload = 10,
  SignalingFromServerPayload = 11,
  SignalingFromClientJoinPayload = 12,
  PingPayload = 13,
  PongPayload = 14,
  BatchPullRoomHistoryPayload = 15
}

// Minimal placeholder for User
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
