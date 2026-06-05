import { Constants, NavHeaderBar } from 'common';
import { UserInfo } from 'base_common';
import { BorrowCard } from '../comp/BorrowCard';

@ComponentV2
export struct BorrowPage {
  @Local userInfo: UserInfo | undefined = undefined;

  async aboutToAppear(): Promise<void> {
    this.userInfo = AppStorage.get('userInfo') as UserInfo;
  }

  build() {
    NavDestination() {
      Stack({ alignContent: Alignment.TopStart }) {
        Column() {
          NavHeaderBar({
            title: $r('app.string.borrow_card'),
            hasBackButton: true,
            hasBgColor: true,
            isMainPage: true,
          });
          BorrowCard({ userInfo: this.userInfo })
            .padding({ top: 30, left: 12, right: 12 })
        }
        .alignItems(HorizontalAlign.Start)
        .width(Constants.FULL_SIZE)

      }
      .width(Constants.FULL_SIZE)
      .height(Constants.FULL_SIZE)
      .backgroundColor($r('sys.color.background_secondary'))
    }.hideTitleBar(true)
  }
}