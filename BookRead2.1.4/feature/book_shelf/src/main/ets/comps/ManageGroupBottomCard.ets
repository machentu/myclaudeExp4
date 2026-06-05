import { Constants } from 'common';

@ComponentV2
export struct ManageGroupBottomCard {
  @Param isIconLight: boolean = false
  @Event delete: () => void = () => {
  };
  @Event groupChange: () => void = () => {
  };

  build() {
    Row() {
      Column({ space: 5 }) {
        Image(this.isIconLight ? $r('app.media.ic_public_folder_thick') : $r('app.media.ic_public_folder_thin'))
          .height(24)
          .width(24)
          .margin({ top: 5 })
        Row() {
          Text($r('app.string.change_group'))
            .fontSize($r('sys.float.Caption_L'))
            .fontColor($r('sys.color.font_primary'))
            .opacity(this.isIconLight ? 1 : 0.4)
            .fontWeight(FontWeight.Regular)
        }
      }

      .onClick(() => {
        this.groupChange();
      })
      .height(70)
      .margin({ left: 50 })

      Column({ space: 5 }) {
        Image(this.isIconLight ? $r('app.media.ic_public_trash_thick') : $r('app.media.ic_public_trash_thin'))
          .height(24)
          .width(24)
          .margin({ top: 5 })
        Row() {
          Text($r('app.string.delete'))
            .fontSize($r('sys.float.Caption_L'))
            .fontColor($r('sys.color.multi_color_08'))
            .opacity(this.isIconLight ? 1 : 0.4)
            .fontWeight(FontWeight.Regular)
        }
      }
      .onClick(() => {
        this.delete();
      })
      .height(70)
      .margin({
        right: 50,
      })
    }.height(70)
    .width(Constants.FULL_SIZE)
    .justifyContent(FlexAlign.SpaceBetween)
    .expandSafeArea([SafeAreaType.SYSTEM], [SafeAreaEdge.TOP, SafeAreaEdge.BOTTOM])
  }
}