export enum ChannelType {
  HUAWEI_PAY = 0,
  ALI_PAY = 1,
  WECHAT_PAY = 2
}

export class WxExtraInfo {
  partnerId: string = ''
  appId: string = ''
  packageValue: string = ''
  prepayId: string = ''
  nonceStr: string = ''
  timeStamp: string = ''
  sign: string = ''
  extData: string = ''
}

export class ChannelInfo {
  channelType: ChannelType = ChannelType.HUAWEI_PAY
  name: string = '';
  appId?: string = '';
  icon: ResourceStr = '';
  preOrderInfo: string | WxExtraInfo = '';
  event?: () => void = () => {}
} 