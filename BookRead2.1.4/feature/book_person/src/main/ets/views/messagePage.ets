import { BreakpointStorage, BreakpointSystem, TCRouter } from 'common'
import { PushKit } from 'custom_push'


@ComponentV2
export struct messagePage {
  @Local curBreakpoint: BreakpointStorage = BreakpointSystem.currentBreakpoint
  build() {
    NavDestination() {
      PushKit({
        curBreakpoint:this.curBreakpoint.value,
        backPop: () => {
          TCRouter.pop()
        }
      })
    }
    .hideTitleBar(true)
    .hideBackButton(true)
  }
}