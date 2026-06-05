import { WxExtraInfo } from 'aggregated_payment';
import { MemberPackage } from '../model/Member';

@ObservedV2
export class MemberCenterVM {
  @Trace memberPackages: MemberPackage[] = [];
  @Trace selectId: number = 0;
  @Trace selectCheckbox: boolean = false;
  @Trace isShow: boolean = false;
  appId: string = '';
  @Trace aliOrderStr: string = '';
  wxOrderReq: WxExtraInfo = new WxExtraInfo();
  hwOrderStr: string = '';
  @Trace lastPurchaseProductExpireTime: string = '';

  getMemberPackages() {
    // 模拟会员套餐数据
    this.memberPackages = [
      {
        id: 1,
        title: '连续包月',
        price: 29,
        description: '每天仅需0.8元',
      },
      {
        id: 2,
        title: '会员月卡',
        price: 39,
        description: '畅想vip权益31天',
      },
      {
        id: 3,
        title: '会员季卡',
        price: 188,
        description: '低至0.93元/天',
      },
      {
        id: 4,
        title: '会员年卡',
        price: 599,
        description: '畅想vip权益365天',
      }
    ];

    if (this.memberPackages.length > 0) {
      this.selectId = this.memberPackages[0].id;
    }
  }
} 