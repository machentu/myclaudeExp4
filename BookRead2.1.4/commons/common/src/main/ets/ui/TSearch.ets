import { TCRouter } from '../comp/TCRouter'
import { Constants } from '../constant/Constants'

@ComponentV2
export struct TSearch {
  @Param widthLength: string = '95%';
  build() {
    Row() {
      Search({
        placeholder: $r('app.string.search_placeholder'),
      })
        .focusable(false)
        .placeholderColor($r('sys.color.font_secondary'))
        .placeholderFont({ size: $r('sys.float.Body_L') })
        .textFont({ size: $r('sys.float.Body_L') })
        .borderRadius(20)
        .onClick(() => {
          TCRouter.push(Constants.SEARCH_ROUTE);
        })
    }
    .width(this.widthLength)
    .height(40)
    .clip(true)
  }
}