import { WxExtraInfo } from 'aggregated_payment';
import { RechargePackage } from '../model/Member';

@ObservedV2
export class RechargeVM {
  @Trace rechargePackages: RechargePackage[] = [];
  @Trace selectId: number = 0;
  @Trace selectCheckbox: boolean = false;
  @Trace isShow: boolean = false;
  @Trace wxOrderReq: WxExtraInfo = new WxExtraInfo();
  @Trace aliOrderStr: string = '';
  @Trace hwOrderStr: string = '';

  getRechargePackages() {
    // 模拟充值套餐数据
    this.rechargePackages = [
      {
        id: 1,
        coins: 60,
        price: 6,
        bonus: 0,
        isRecommended: false
      },
      {
        id: 2,
        coins: 300,
        price: 30,
        bonus: 60,
        isRecommended: false
      },
      {
        id: 3,
        coins: 680,
        price: 68,
        bonus: 220,
        isRecommended: true
      },
      {
        id: 4,
        coins: 1280,
        price: 128,
        bonus: 520,
        isRecommended: false
      },
      {
        id: 5,
        coins: 3280,
        price: 328,
        bonus: 1620,
        isRecommended: false
      },
      {
        id: 6,
        coins: 6480,
        price: 648,
        bonus: 3520,
        isRecommended: false
      }
    ];

    if (this.rechargePackages.length > 0) {
      this.selectId = this.rechargePackages[0].id;
    }
  }
} 