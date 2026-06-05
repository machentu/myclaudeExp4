import { UserInfoModel } from '../model/UserInfoModel';
import { UserInfo } from 'base_common';
export class UserInfoUtil {
  private static _userInfoModel: UserInfoModel = new UserInfoModel();

  // 获取可观察的用户信息模型
  public static getUserInfoModel(): UserInfoModel {
    return UserInfoUtil._userInfoModel;
  }

  // 从AppStorage加载用户信息并同步到模型
  public static loadUserInfo(): void {
    const userInfo = AppStorage.get('userInfo') as UserInfo;
    UserInfoUtil._userInfoModel.updateFromUserInfo(userInfo);
  }

  // 更新会员状态
  public static updateMembership(): void {
    UserInfoUtil._userInfoModel.isMembership = true;
    UserInfoUtil.saveToAppStorage();
    AppStorage.setOrCreate('membershipStatus', true);
  }

  // 取消会员状态
  public static cancelMembership(): void {
    UserInfoUtil._userInfoModel.isMembership = false;
    UserInfoUtil.saveToAppStorage();
    AppStorage.setOrCreate('membershipStatus', false);
  }

  // 更新完整用户信息
  public static updateUserInfo(userInfo: UserInfo): void {
    UserInfoUtil._userInfoModel.updateFromUserInfo(userInfo);
    UserInfoUtil.saveToAppStorage();
  }

  // 保存到AppStorage
  private static saveToAppStorage(): void {
    const userInfo = UserInfoUtil._userInfoModel.toUserInfo();
    AppStorage.set('userInfo', userInfo);
  }

  // 初始化，从AppStorage加载数据
  public static init(): void {
    UserInfoUtil.loadUserInfo();
  }
}