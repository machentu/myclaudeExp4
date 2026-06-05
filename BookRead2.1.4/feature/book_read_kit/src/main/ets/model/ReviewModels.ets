// 评论数据模型
export interface ReviewInfo {
  id: string;
  userId: string;
  username: string;
  avatar: ResourceStr;
  content: ResourceStr;
  rating: number;
  createTime: string;
  isVip: boolean;
  vipLevel?: string;
  isExpanded?: boolean;
  // 新增书籍信息
  bookInfo?: BookBasicInfo;
}

// 书籍基本信息
export interface BookBasicInfo {
  id: string;
  name: string;
  coverUrl: ResourceStr;
  category: string;
}

// 评论筛选类型
export enum ReviewFilterType {
  ALL = 'all',
  LATEST = 'latest'
}

// 用户阅读状态
export interface ReadingStatus {
  isCompleted: boolean;
  progress: number;
}

// 评分等级描述
export enum RatingDescription {
  ONE_STAR = '非常差',
  TWO_STAR = '很差', 
  THREE_STAR = '一般',
  FOUR_STAR = '很好',
  FIVE_STAR = '非常棒'
}

// 获取评分描述
export function getRatingDescription(rating: number): string {
  switch (rating) {
    case 1:
      return RatingDescription.ONE_STAR;
    case 2:
      return RatingDescription.TWO_STAR;
    case 3:
      return RatingDescription.THREE_STAR;
    case 4:
      return RatingDescription.FOUR_STAR;
    case 5:
      return RatingDescription.FIVE_STAR;
    default:
      return '给星星打个分';
  }
}

// 评论数据接口
export interface ReviewData {
  id: string;
  avatar: ResourceStr;
  username: string;
  content: ResourceStr;
  rating: number;
  // 新增书籍信息
  bookInfo?: BookBasicInfo;
} 