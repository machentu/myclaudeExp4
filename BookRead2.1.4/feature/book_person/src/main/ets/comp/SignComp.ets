import { Constants, SliderList, SliderModel, TaskList } from 'common'
import { AppStorageV2, ComponentContent } from '@kit.ArkUI'
import { BusinessError, emitter } from '@kit.BasicServicesKit'
import { hilog } from '@kit.PerformanceAnalysisKit'
import { UserInfo } from 'base_common';

const DOMAIN_NUMBER: number = 0xFF00;
const TAG = 'Sign';

@ComponentV2
export struct SignComp {
  @Consumer('index') index: number = 0;
  @Local point: number = 0;
  @Local signIn: number = AppStorage.get('signIn') || 0;
  private currentSliderModel: SliderModel | null = null
  private contentNode: ComponentContent<DialogClass> | null = null
  uiContext = this.getUIContext();
  promptAction = this.uiContext.getPromptAction();
  @Local arr: SliderList = AppStorageV2.connect(SliderList, 'SliderList', () => new SliderList())!
  @Local userInfo: UserInfo = AppStorage.get('userInfo') as UserInfo;

  calculateTomorrowDate(date: number) {
    let today = new Date()
    let next = new Date(today)
    next.setDate(today.getDate() + date) // 增加一天

    // 格式化月份和日期
    const MONTH = (next.getMonth() + 1).toString()
    const DAY = next.getDate().toString()
    let nextDate = `${MONTH}.${DAY}`
    return nextDate;
  }

  aboutToAppear(): void {
  }

  closeDialog = () => {
    hilog.info(DOMAIN_NUMBER, TAG, 'close dialog')
    this.promptAction.closeCustomDialog(this.contentNode)
    this.point += this.currentSliderModel!.value
    if (this.index <= 6) {
      this.index++
    }
  }

  @Builder
  showSignIn() {
    Column() {
      Text() {
        Span('本轮已经连续签到')
        Span(this.signIn.toString())
        Span('天')
      }

      Row() {
        Flex({ direction: FlexDirection.Row }) {
          ForEach(this.arr.sliderList, (item: SliderModel, index: number) => {
            ListItem() {
              CustomSlider({
                sliderModel: item,
                booLeft: this.index > index, //判断left左侧
                booRight: this.index - 1 > index,
                index: this.index
              })
            }
            .onClick(() => {
              if (item.day === Constants.DATE &&
                this.arr.sliderList[this.index === 7 ? this.arr.sliderList.length - 1 : this.index].isClick === false) {
                try {
                  if (this.userInfo?.bookCoins !== undefined) {
                    this.userInfo.bookCoins += item.value
                    emitter.emit(Constants.COINS_CHANGE)
                  }

                  this.promptAction.showToast({ message: `成功签到，积分+${item.value}` })
                  if (this.signIn !== undefined) {
                    AppStorage.setOrCreate('signIn', this.signIn += 1)
                    this.arr.sliderList[this.index === 7 ? this.arr.sliderList.length - 1 : this.index].isClick = true;
                  } else {
                    AppStorage.setOrCreate('signIn', 1)
                  }
                } catch (error) {
                  let message = (error as BusinessError).message;
                  let code = (error as BusinessError).code;
                  hilog.error(DOMAIN_NUMBER, TAG, `OpenCustomDialog args error code is ${code}, message is ${message}`);
                }
              }
            })
          }, (item: SliderModel) => new Date().toString())
        }
        .layoutWeight(1)
        .padding({
          top: 10,
        })
      }
      .alignSelf(ItemAlign.Center)
    }
    .borderRadius(12)
    .padding({
      left: 10,
      right: 10,
      top: 20,
      bottom: 6
    })
    .margin({
      top: 12,
      bottom: 12
    })
    .backgroundColor($r('sys.color.comp_background_list_card'))
  }

  build() {
    Column() {
      Text() {
        Span('本轮已经连续签到')
        Span(this.signIn.toString())
        Span('天')
      }

      Row() {
        Flex({ direction: FlexDirection.Row }) {
          ForEach(this.arr.sliderList, (item: SliderModel, index: number) => {
            ListItem() {
              CustomSlider({
                sliderModel: item,
                booLeft: this.index > index, //判断left左侧
                booRight: this.index - 1 > index,
                index: this.index
              })
            }
            .onClick(() => {
              if (item.day === Constants.DATE &&
                this.arr.sliderList[this.index === 7 ? this.arr.sliderList.length - 1 : this.index].isClick === false) {
                try {
                  if (this.userInfo?.bookCoins !== undefined) {
                    this.userInfo.bookCoins += item.value
                    emitter.emit(Constants.COINS_CHANGE)
                  }

                  this.promptAction.showToast({ message: `成功签到，积分+${item.value}` })
                  if (this.signIn !== undefined) {
                    AppStorage.setOrCreate('signIn', this.signIn += 1)
                    this.arr.sliderList[this.index === 7 ? this.arr.sliderList.length - 1 : this.index].isClick = true;
                  } else {
                    AppStorage.setOrCreate('signIn', 1)
                  }
                } catch (error) {
                  let message = (error as BusinessError).message;
                  let code = (error as BusinessError).code;
                  hilog.error(DOMAIN_NUMBER, TAG, `OpenCustomDialog args error code is ${code}, message is ${message}`);
                }
              }
            })
          }, (item: SliderModel) => new Date().toString())
        }
        .width(Constants.FULL_WIDTH)
        .padding({
          top: 10,
        })
      }
      .alignSelf(ItemAlign.Center)
    }
    .borderRadius(16)
    .padding({
      left: 10,
      right: 10,
      top: 20,
      bottom: 12
    })
    .backgroundColor($r('sys.color.comp_background_primary'))
  }
}

@ComponentV2
export struct CustomSlider {
  @Param @Require sliderModel: SliderModel;
  @Param @Require booLeft: boolean;
  @Param @Require booRight: boolean;
  @Param index: number = 0;

  build() {
    Column() {
      Column() {
        Image(this.sliderModel.isClick ? $r('app.media.sign_on') : $r('app.media.sign_off'))
          .height(20)
          .width(20)
        Text(`+${this.sliderModel.value}`)
          .fontSize(8)
          .fontWeight(FontWeight.Normal)
          .margin({ top: 2 })
          .fontColor(this.sliderModel.isClick ? $r('sys.color.font_on_primary') :
            $r('sys.color.font_secondary'))
      }
      .width(40)
      .height(40)
      .borderRadius(8)
      .alignItems(HorizontalAlign.Center)
      .justifyContent(FlexAlign.Center)
      .linearGradient({
        direction: GradientDirection.Bottom,
        angle: 0,
        colors: this.sliderModel.isClick ? [[0xFDEDD0, 0.0], [0xFFAA15, 1.0]] :
          [[$r('app.color.sign_card_bottom'), 0.0], [$r('app.color.sign_card_top'), 1.0]]
      })

      Text(this.sliderModel.isClick ? '已签' : '未签')
        .fontSize(8)
        .width(40)
        .textAlign(TextAlign.Center)
        .fontColor(this.sliderModel.isClick ? $r('app.color.sign_day_color_on') :
          $r('sys.color.font_secondary'))
    }
    .width(Constants.CUSTOM_SLIDER_WIDTH)
    .height(Constants.CUSTOM_SLIDER_HEIGHT)
  }
}

export class DialogClass {
  data: string | number | object

  constructor(data: string | number | object) {
    this.data = data;
  }
}


@Builder
export function SignInSuccessDialogBuilder(dialogClass: DialogClass) {
  Column() {
    Text('签到成功')
      .fontSize(18)
      .fontWeight(FontWeight.Medium)
      .margin({
        top: 10,
        bottom: 10
      })

    Blank()

    Text('请每天保持签到')
      .fontSize(14)
      .fontWeight(FontWeight.Regular)
      .fontColor('#66000000')
      .margin({
        bottom: 15
      })

  }
  .width(300)
  .height(260)
  .backgroundColor($r('sys.color.comp_background_list_card'))
  .borderRadius(16)
  .margin({
    top: 10
  })
}