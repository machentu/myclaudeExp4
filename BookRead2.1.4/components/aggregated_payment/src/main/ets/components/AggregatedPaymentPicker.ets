import { common } from '@kit.AbilityKit';
import { ChannelType, WxExtraInfo, ChannelInfo } from '../model/PaymentModels';
import { AggregatedPaymentVM } from '../utils/AggregatedPaymentVM';

@ComponentV2
export struct AggregatedPaymentPicker {
  vm: AggregatedPaymentVM = AggregatedPaymentVM.instance;
  context: common.UIAbilityContext = this.getUIContext().getHostContext() as common.UIAbilityContext;
  @Local isShow: boolean = false;
  @Local checkedType: number = 0;
  @Local wxPayResultText: string = '';
  @Local preOrderInfo: string | WxExtraInfo = '';
  @Local appId: string = '';
  /**
   * 支付渠道及支付渠道信息
   */
  @Param channelInfo: ChannelInfo[] = [];
  /**
   * 支付金额
   */
  @Param amount: number = 0;
  /**
   * 支付成功回调
   */
  @Param paySuccessEvent: (type: ChannelType) => void = () => {
  };

  aboutToAppear(): void {
    this.checkedType = this.channelInfo[0].channelType;
  }

  @Builder
  paymentChannelPicker() {
    Column() {
      if (this.amount > 0) {
        Text() {
          Span('支付金额：').fontSize(14).fontColor($r('sys.color.font_secondary'))
          Span('￥').fontSize(14).fontColor('#E39331')
          Span(this.amount.toFixed(2)).fontSize(18).fontWeight(FontWeight.Bold).fontColor('#E39331')
        }
        .width('100%')
        .padding({ top: 16, bottom: 8, left: 12, right: 12 })
        .textAlign(TextAlign.Start)
        
        Divider()
          .margin({ left: 12, right: 12, bottom: 8 })
      }
      
      ForEach(this.channelInfo, (item: ChannelInfo) => {
        Row() {
          Row({ space: 16 }) {
            Image(item.icon).size({ width: 24, height: 24 });
            Text(item.name).fontWeight(500);
          };

          Checkbox({ name: 'channelCheckBox', group: 'checkboxGroup' })
            .select(this.checkedType === item.channelType)
            .selectedColor(0xed6f21)
            .shape(CheckBoxShape.CIRCLE)
            .onChange((value: boolean) => {
              if (value) {
                this.checkedType = item.channelType;
                this.preOrderInfo = item.preOrderInfo;
                if (item.appId) {
                  this.appId = item.appId;
                }
              }
            });
        }.width('100%').justifyContent(FlexAlign.SpaceBetween).padding(12);
      }, (item: ChannelInfo) => item.channelType.toString())
      
      Button('确定付款')
        .width('100%')
        .backgroundColor('#E39331')
        .onClick(() => {
          if (this.checkedType === ChannelType.ALI_PAY) {
            this.vm.aliPay(this.paySuccessEvent, this.preOrderInfo);
          } else if (this.checkedType === ChannelType.WECHAT_PAY) {
            let channel = this.channelInfo.find((item) => item.channelType === ChannelType.WECHAT_PAY);
            if (channel?.event) {
              channel.event();
            }
            this.vm.weChatPay(this.preOrderInfo, this.appId);
          } else {
            this.vm.huaweiPay(this.context, this.preOrderInfo, this.paySuccessEvent);
          }
        });
    }.padding(12);
  }

  build() {
    NavDestination() {
      Column() {
        this.paymentChannelPicker();
      }.height('100%').justifyContent(FlexAlign.Center);
    }.hideTitleBar(true).hideToolBar(true);
  }
} 