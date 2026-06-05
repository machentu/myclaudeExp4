import { BreakpointStorage, BreakpointSystem, BreakpointTypeEnum, Constants, NavHeaderBar } from 'common';
import { RechargeRecord } from 'common/src/main/ets/model/Member';
import { RechargeRecordVM } from 'common/src/main/ets/viewmodel/RechargeRecordVM';

@Preview
@ComponentV2
export struct RechargeRecordPage {
  @Local curBreakpoint: BreakpointStorage = BreakpointSystem.currentBreakpoint
  vm: RechargeRecordVM = new RechargeRecordVM();
  bottomRectHeight: number = AppStorage.get('bottomRectHeight') as number

  aboutToAppear(): void {
    this.vm.getRechargeRecords();
  }

  build() {
    NavDestination() {
      Column() {
        NavHeaderBar({
          title: '充值记录',
          hasBackButton: true,
          hasBgColor: false,
          isMainPage: false,
        })
        Column() {
          if (this.vm.isLoading) {
            Column() {
              LoadingProgress()
                .color($r('sys.color.icon_tertiary'))

              Text('加载中...')
                .fontSize(14)
                .fontColor($r('sys.color.font_secondary'))
                .margin({ top: 16 })
            }
            .width(Constants.FULL_SIZE)
            .height(Constants.FULL_SIZE)
            .justifyContent(FlexAlign.Center)
            .alignItems(HorizontalAlign.Center)
          } else if (this.vm.rechargeRecords.length === 0) {
            // 空状态
            Column() {
              Column({ space: 16 }) {
                Image($r('app.media.NoRecord'))
                  .width(120)
                  .height(120)
                  .fillColor('#CCCCCC')

                Text('暂无充值记录')
                  .fontSize(16)
                  .fontColor($r('sys.color.font_secondary'))
              }
              .justifyContent(FlexAlign.Center)
              .alignItems(HorizontalAlign.Center)
              .height(400)
            }
            .layoutWeight(1)
            .width(this.curBreakpoint.value === BreakpointTypeEnum.LG ? '70%' : '100%')
            .height('100%')
            .backgroundColor($r('sys.color.comp_background_list_card'))
            .borderRadius(16)
          } else {
            // 充值记录列表
            List({ space: 0 }) {
              ForEach(this.vm.rechargeRecords, (record: RechargeRecord, index: number) => {
                ListItem() {
                  this.rechargeRecordItem(record)
                }
                .height(100)
              }, (record: RechargeRecord) => record.id)
            }
            .width(this.curBreakpoint.value === BreakpointTypeEnum.LG ? '70%' : '100%')
            .layoutWeight(1)
            .scrollBar(BarState.Off)
            .edgeEffect(EdgeEffect.Spring)
            .backgroundColor($r('sys.color.comp_background_list_card'))
            .borderRadius(16)
            .divider({
              strokeWidth: 1,
              color: '#F0F0F0',
              startMargin: 68,
              endMargin: 16
            })
          }
        }
        .width(Constants.FULL_SIZE)
        .height(Constants.FULL_SIZE)
        .layoutWeight(1)
        .padding({
          bottom: this.bottomRectHeight,
          left: 12,
          right: 12,
          top: 12
        })
        .backgroundColor($r('sys.color.background_secondary'))
      }
    }
    .hideTitleBar(true)
  }

  @Builder
  rechargeRecordItem(record: RechargeRecord) {
    Row() {
      Image($r('app.media.RechargeRecord'))
        .width(40)
        .height(40)
        .margin({ right: 12 })

      Column({ space: 6 }) {
        Row() {
          Column({ space: 2 }) {
            Text(record.date)
              .fontSize(15)
              .fontWeight(FontWeight.Medium)
              .fontColor($r('sys.color.font_primary'))
              .alignSelf(ItemAlign.Start)

            Text(`订单编号：${record.orderId}`)
              .fontSize(11)
              .fontColor($r('sys.color.font_secondary'))
              .alignSelf(ItemAlign.Start)
          }
          .alignItems(HorizontalAlign.Start)
          .layoutWeight(1)

          Column({ space: 2 }) {
            Text(`+${record.totalCoins}书币`)
              .fontSize(15)
              .fontWeight(FontWeight.Medium)
              .fontColor('#E39331')

            Text() {
              Span('实付：￥')
              Span(record.amount.toFixed(2))
            }
            .fontSize(11)
            .fontColor($r('sys.color.font_secondary'))
          }
          .alignItems(HorizontalAlign.End)
        }
        .width(Constants.FULL_SIZE)

        // 显示基础书币和赠送书币信息
        Text(`基础书币：${record.coins} + 赠送书币：${record.bonus}`)
          .fontSize(11)
          .fontColor($r('sys.color.font_tertiary'))
          .alignSelf(ItemAlign.Start)
      }
      .layoutWeight(1)
      .justifyContent(FlexAlign.Center)
    }
    .padding({
      left: 16,
      right: 16,
      top: 12,
      bottom: 12
    })
    .width(Constants.FULL_SIZE)
    .height(Constants.FULL_SIZE)
    .alignItems(VerticalAlign.Center)
  }
} 