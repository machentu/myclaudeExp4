import { hilog } from '@kit.PerformanceAnalysisKit';
import { BusinessError, emitter } from '@kit.BasicServicesKit';
import { Constants, TCRouter, UserInfoModel, UserInfoUtil } from 'common';
import { UserInfo } from 'base_common';
import { AvatarIcon, SelectAvatarCard } from './SelectAvatarCard';

@ComponentV2
export struct UserMsgCard {
  @Local userName: string = ''
  @Local userTel: string = ''
  @Local isLogin: boolean = false
  @Local avatarIcon: AvatarIcon = new AvatarIcon('');
  @Local userInfoModel: UserInfoModel = UserInfoUtil.getUserInfoModel();

  aboutToAppear(): void {
    this.loginPost();
    try {
      emitter.on(Constants.LOGIN_CHANGE, () => {
        this.loginPost();
      });
    } catch (error) {
      console.error('Failed to emitter on login change. Cause: ' + JSON.stringify(error));
    }
    this.change();
  }

  loginPost() {
    let userInfo: UserInfo = AppStorage.get('userInfo') as UserInfo;
    if (userInfo) {
      this.userInfoModel.updateFromUserInfo(userInfo);
      this.userName = userInfo.nickName;
      this.userTel = userInfo.telephone as string;
      this.avatarIcon.avatarUrl = userInfo.avatarUrl as string;
      this.isLogin = true;
    } else {
      this.userName = '';
      this.userTel = '';
      this.avatarIcon.avatarUrl = '';
      this.isLogin = false;
    }
  }

  change() {
    if (this.userTel) {
      this.userTel =
        `${this.userTel.substring(0, this.userTel.length - 8)}****${this.userTel.substring(this.userTel.length - 4)}`
    }
    this.isLogin = !!this.userTel
  }

  build() {
    Row() {
      Row() {
        if (this.isLogin) {
          SelectAvatarCard({ avatarIcon: this.avatarIcon, isLogin:this.isLogin })
        } else {
          Image($r('app.media.ic_default_hd'))
            .width(64)
            .height(64)
            .borderRadius(50)
            .onClick(() => {
              TCRouter.push(Constants.LOGIN_ROUTE);
            })
        }

        Column() {
          if (this.isLogin) {
            Text(this.userName)
              .fontSize(18)
              .fontWeight(FontWeight.Medium)
              .fontColor($r('sys.color.font_primary'))

            Row() {
              Image(this.userInfoModel.isMembership ? $r('app.media.ic_vip') : $r('app.media.ic_vip_off'))
                .width(20)
                .height(18)
                .fillColor(this.userInfoModel.isMembership ? '#F9A01E' : '#999999')
              Text(this.userInfoModel.isMembership ? $r('app.string.vip_member') : $r('app.string.no_member'))
                .fontSize(12)
                .fontColor($r('sys.color.font_primary'))
                .margin({ left: 8 })
            }
            .width(90)
            .padding({
              top: 6,
              bottom: 6,
              left: 8,
              right: 8,
            })
            .borderRadius(8)
            .backgroundColor(this.userInfoModel.isMembership ? '#f8fcf2cc' : $r('sys.color.background_secondary'))
            .margin({ top: 12, left: 1 })
          } else {
            Text($r('app.string.no_relative')).fontSize(18)
              .fontWeight(FontWeight.Medium)
              .fontColor($r('sys.color.font_secondary'))
              .onClick(() => {
                TCRouter.push(Constants.LOGIN_ROUTE);
              })
          }
        }
        .alignItems(HorizontalAlign.Start)
        .margin({ left: 12 })
      }
    }
    .width(Constants.FULL_SIZE)
    .justifyContent(FlexAlign.SpaceBetween)
    .height(112)
    .borderRadius(16)
    .padding({
      left: 16,
      right: 16,
      top: 24,
      bottom: 24,
    })
  }

  // 错误处理
  dealAllError(error: BusinessError<Object>): void {
    hilog.error(0x0000, 'testTag',
      `Failed to login, errorCode is ${error.code}, errorMessage is ${error.message}`);
    this.getUIContext().getPromptAction().showToast({ message: '账号关联失败' })
  }
}
