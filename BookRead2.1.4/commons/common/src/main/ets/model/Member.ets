// 会员套餐
export interface MemberPackage {
  id: number
  title: string;
  price: number
  description: string;
}

// 充值套餐
export interface RechargePackage {
  id: number
  coins: number;  // 书币数量
  price: number   // 价格
  bonus?: number; // 赠送书币数量
  isRecommended?: boolean; // 是否推荐
}

// 充值记录
export interface RechargeRecord {
  id: string;        // 唯一标识
  orderId: string;   // 订单编号
  coins: number;     // 书币数量
  bonus: number;     // 赠送书币
  totalCoins: number; // 总书币 (coins + bonus)
  amount: number;    // 实付金额
  timestamp: number; // 充值时间戳
  date: string;      // 格式化的日期时间
} 