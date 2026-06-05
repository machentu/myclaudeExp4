import { Constants, NavHeaderBar, TCRouter } from 'common';

@ComponentV2
export struct MemberAgreementPage {
  scroller: Scroller = new Scroller();

  build() {
    NavDestination() {
      Stack({ alignContent: Alignment.TopStart }) {
        Column() {
          NavHeaderBar({
            title: $r('app.string.member_agreement'),
            hasBackButton: true,
            hasBgColor: true,
            isMainPage: true,
          })

          Scroll(this.scroller) {
            Column({ space: 16 }) {
              Text('会员服务协议')
                .fontSize(20)
                .fontWeight(FontWeight.Bold)
                .fontColor($r('sys.color.font_primary'))
                .alignSelf(ItemAlign.Start)

              Text('生效日期：2025年1月1日')
                .fontSize(12)
                .fontColor($r('sys.color.font_secondary'))
                .alignSelf(ItemAlign.Start)

              Text('欢迎您使用我们的会员服务！请仔细阅读以下条款和条件。')
                .fontSize(14)
                .fontColor($r('sys.color.font_primary'))
                .lineHeight(22)

              Column({ space: 12 }) {
                Text('1. 会员权益')
                  .fontSize(16)
                  .fontWeight(FontWeight.Medium)
                  .fontColor($r('sys.color.font_primary'))
                  .alignSelf(ItemAlign.Start)

                Text('1.1 无广告体验：享受无广告干扰的阅读环境。\n1.2 优先咨询：在咨询服务中享有优先处理权。\n1.3 专属客服：获得专属客服团队的贴心服务。\n1.4 其他权益：根据会员等级享受相应的专属权益。')
                  .fontSize(14)
                  .fontColor($r('sys.color.font_secondary'))
                  .lineHeight(20)
              }
              .alignItems(HorizontalAlign.Start)

              Column({ space: 12 }) {
                Text('2. 会员费用')
                  .fontSize(16)
                  .fontWeight(FontWeight.Medium)
                  .fontColor($r('sys.color.font_primary'))
                  .alignSelf(ItemAlign.Start)

                Text('2.1 会员费用按照您选择的套餐进行收费。\n2.2 费用将在您确认购买时从您的账户中扣除。\n2.3 会员服务一经购买，不支持退款。')
                  .fontSize(14)
                  .fontColor($r('sys.color.font_secondary'))
                  .lineHeight(20)
              }
              .alignItems(HorizontalAlign.Start)

              Column({ space: 12 }) {
                Text('3. 服务期限')
                  .fontSize(16)
                  .fontWeight(FontWeight.Medium)
                  .fontColor($r('sys.color.font_primary'))
                  .alignSelf(ItemAlign.Start)

                Text('3.1 会员服务期限按照您购买的套餐确定。\n3.2 连续包月服务将自动续费，您可以随时取消。\n3.3 服务到期后，会员权益将自动停止。')
                  .fontSize(14)
                  .fontColor($r('sys.color.font_secondary'))
                  .lineHeight(20)
              }
              .alignItems(HorizontalAlign.Start)

              Column({ space: 12 }) {
                Text('4. 用户义务')
                  .fontSize(16)
                  .fontWeight(FontWeight.Medium)
                  .fontColor($r('sys.color.font_primary'))
                  .alignSelf(ItemAlign.Start)

                Text('4.1 您需要遵守相关法律法规和平台规则。\n4.2 不得利用会员权益进行违法违规活动。\n4.3 保护账户安全，不得转让或共享会员权益。')
                  .fontSize(14)
                  .fontColor($r('sys.color.font_secondary'))
                  .lineHeight(20)
              }
              .alignItems(HorizontalAlign.Start)

              Column({ space: 12 }) {
                Text('5. 服务变更')
                  .fontSize(16)
                  .fontWeight(FontWeight.Medium)
                  .fontColor($r('sys.color.font_primary'))
                  .alignSelf(ItemAlign.Start)

                Text('5.1 我们保留调整会员权益和服务内容的权利。\n5.2 重大变更将提前通知您。\n5.3 您可以选择接受变更或取消服务。')
                  .fontSize(14)
                  .fontColor($r('sys.color.font_secondary'))
                  .lineHeight(20)
              }
              .alignItems(HorizontalAlign.Start)

              Column({ space: 12 }) {
                Text('6. 联系我们')
                  .fontSize(16)
                  .fontWeight(FontWeight.Medium)
                  .fontColor($r('sys.color.font_primary'))
                  .alignSelf(ItemAlign.Start)

                Text('如果您对本协议有任何疑问，请通过以下方式联系我们：\n邮箱：xxxxx@huawei.com\n电话：400-xxx-xxxx\n工作时间：周一至周五 9:00-18:00')
                  .fontSize(14)
                  .fontColor($r('sys.color.font_secondary'))
                  .lineHeight(20)
              }
              .alignItems(HorizontalAlign.Start)

              Text('最后更新时间：2025年1月1日')
                .fontSize(12)
                .fontColor($r('sys.color.font_secondary'))
                .alignSelf(ItemAlign.Start)
                .margin({ top: 20 })
            }
            .width(Constants.FULL_SIZE)
            .padding(16)
            .backgroundColor($r('sys.color.comp_background_list_card'))
            .borderRadius(16)
          }
          .edgeEffect(EdgeEffect.Spring)
          .width(Constants.FULL_SIZE)
          .padding({
            left: 12,
            right: 12,
          })
          .margin({ bottom: 100 })
        }
        .alignItems(HorizontalAlign.Start)
        .width(Constants.FULL_SIZE)
        .height(Constants.FULL_SIZE)
      }
      .width(Constants.FULL_SIZE)
      .height(Constants.FULL_SIZE)
      .backgroundColor($r('sys.color.background_secondary'))
    }
    .hideTitleBar(true)
    .width(Constants.FULL_SIZE)
    .height(Constants.FULL_SIZE)
    .backgroundColor($r('sys.color.background_secondary'))
  }
} 