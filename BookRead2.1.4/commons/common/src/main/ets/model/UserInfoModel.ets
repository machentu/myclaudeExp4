import { UserInfo } from 'base_common';
@ObservedV2
export class UserInfoModel {
  @Trace id: string = ''
  @Trace nickName: string = ''
  @Trace avatarUrl?: string
  @Trace birthday?: string
  @Trace telephone?: string
  @Trace borrowCardStart?: number
  @Trace borrowCardEnd?: number
  @Trace totalReading: number = 0
  @Trace totalListening: number = 0
  @Trace isLogin?: boolean
  @Trace isMembership: boolean = false
  @Trace bookCoins: number = 0

  // 从UserInfo转换
  updateFromUserInfo(userInfo: UserInfo | undefined) {
    if (userInfo) {
      this.id = userInfo.id;
      this.nickName = userInfo.nickName;
      this.avatarUrl = userInfo.avatarUrl;
      this.birthday = userInfo.birthday;
      this.telephone = userInfo.telephone;
      this.borrowCardStart = userInfo.borrowCardStart;
      this.borrowCardEnd = userInfo.borrowCardEnd;
      this.totalReading = userInfo.totalReading;
      this.totalListening = userInfo.totalListening;
      this.isLogin = userInfo.isLogin;
      this.isMembership = userInfo.isMembership;
      this.bookCoins = userInfo.bookCoins;
    }
  }

  // 转换为UserInfo
  toUserInfo(): UserInfo {
    const userInfo = new UserInfo();
    userInfo.id = this.id;
    userInfo.nickName = this.nickName;
    userInfo.avatarUrl = this.avatarUrl;
    userInfo.birthday = this.birthday;
    userInfo.telephone = this.telephone;
    userInfo.borrowCardStart = this.borrowCardStart;
    userInfo.borrowCardEnd = this.borrowCardEnd;
    userInfo.totalReading = this.totalReading;
    userInfo.totalListening = this.totalListening;
    userInfo.isLogin = this.isLogin;
    userInfo.isMembership = this.isMembership;
    userInfo.bookCoins = this.bookCoins;
    return userInfo;
  }
}