import { Constants } from 'common'


@ComponentV2
export struct MyBalanceCard {
  build() {
    Row(){
      Column(){
        Text('我的账户')
          .width('100%')
          .height(21)
          .fontSize(16)
          .textAlign(TextAlign.Start)

        Divider()
          .width(Constants.FULL_SIZE)
          .margin({
            top:12,
            bottom:12
          })

        Row(){
          Column({space:4}){
            Text('100')
              .fontSize(18)
            Text('书币')
              .fontSize(12)
          }
          .width(72)

          Column(){
            Button(){
              Text('充值')
                .fontSize($r('sys.float.Body_M'))
                .height(20)
                .width(56)
                .fontColor($r('sys.color.font_on_primary'))
                .textAlign(TextAlign.Center)
            }
            .position({ x: 'right', y: 'bottom' })
            .backgroundColor('#E84026')
            .align(Alignment.BottomEnd)
            .margin({top:16})
            .padding({
              left:8,
              right:8,
              top:4,
              bottom:4
            })
          }
          .width(72)

        }
        .justifyContent(FlexAlign.SpaceBetween)
        .width(Constants.FULL_SIZE)
      }
    }
    .height(114)
    .width(Constants.FULL_SIZE)
    .borderRadius(16)
    .backgroundColor($r('sys.color.background_primary'))
    .padding({
      top:12,
      bottom:12,
      left:12,
      right:12
    })
  }
}