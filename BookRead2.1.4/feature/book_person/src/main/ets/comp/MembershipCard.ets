import { Constants, TCRouter, UserInfoModel, UserInfoUtil } from 'common';
import { emitter } from '@kit.BasicServicesKit';
import { UserInfo } from 'base_common';

@ComponentV2
export struct MembershipCard {
  @Local userInfoModel: UserInfoModel = UserInfoUtil.getUserInfoModel();
  @Local isLogin: boolean = false;

  aboutToAppear(): void {
    this.loadUserInfo();
    try {
      emitter.on(Constants.LOGIN_CHANGE, () => {
        this.loadUserInfo();
      });
    } catch (e) {
      console.error('Failed to emitter on login change. Cause: ' + JSON.stringify(e));
    }
  }

  loadUserInfo() {
    const userInfoFromStorage: UserInfo = AppStorage.get('userInfo') as UserInfo;
    if (userInfoFromStorage) {
      this.userInfoModel.updateFromUserInfo(userInfoFromStorage);
      this.isLogin = true;
    } else {
      this.isLogin = false;
    }
  }

  @Builder
  userTagBuilder() {
    Row() {
      Image(this.userInfoModel.isMembership ? $r('app.media.ic_book_list') : $r('app.media.ic_book_list'))
        .width(20)
        .height(18)
        .fillColor(this.userInfoModel.isMembership ? '#F9A01E' : '#999999')
      Text(this.userInfoModel.isMembership ? $r('app.string.vip_member') : $r('app.string.no_member'))
        .fontSize(12)
        .fontColor($r('sys.color.font_primary'))
        .margin({ left: 8 })
    }
    .width(96)
    .padding({
      top: 6,
      bottom: 6,
      left: 8,
      right: 8,
    })
    .borderRadius(8)
    .backgroundColor(this.userInfoModel.isMembership ? '#FCF2CC' : $r('sys.color.background_secondary'))
    .margin({ top: 4 })
    .visibility(this.isLogin ? Visibility.Visible : Visibility.None)
  }

  @Builder
  membershipBuilder() {
    Column() {
      Text($r('app.string.enjoy_more_services'))
        .fontSize(18)
        .fontWeight(FontWeight.Medium)
        .fontColor('#D17E18')
      Button(this.userInfoModel.isMembership ? $r('app.string.member_center') : $r('app.string.open_member'), 
        { controlSize: ControlSize.SMALL })
        .fontSize($r('sys.float.Body_M'))
        .fontWeight(FontWeight.Medium)
        .fontColor($r('sys.color.font_on_primary'))
        .backgroundColor('#E3922F')
        .margin({ top: 4 })
        .onClick(() => {
          TCRouter.push(Constants.MEMBER_CENTER_ROUTE);
        })
    }
    .width('100%')
    .height(80)
    .alignItems(HorizontalAlign.End)
    .justifyContent(FlexAlign.SpaceBetween)
    .padding(8)
    .backgroundImage($r('app.media.mask'))
    .backgroundImageSize({ width: '100%', height: 80 })
    .backgroundColor('#F0F2F5')
    .borderRadius(16)
    .visibility(this.isLogin ? Visibility.Visible : Visibility.None)
  }

  build() {
    Column({ space: 10 }) {
      this.userTagBuilder()
      this.membershipBuilder()
    }
    .width(Constants.FULL_SIZE)
  }
} 