import { ReviewInfo, ReviewFilterType, ReadingStatus } from '../model/ReviewModels';
import { ReviewDataUtil } from '../utils/ReviewDataUtil';

@ObservedV2
export class WriteReviewVM {
  @Trace userRating: number = 1; // 用户评分，默认1星
  @Trace currentFilter: ReviewFilterType = ReviewFilterType.ALL; // 当前筛选类型
  @Trace readingStatus: ReadingStatus = { isCompleted: false, progress: 45 }; // 阅读状态
  
  // 获取所有评论数据
  getAllReviews(): ReviewInfo[] {
    return ReviewDataUtil.getAllReviews();
  }

  // 获取精彩书评数据
  getWonderfulReviews(): ReviewInfo[] {
    return ReviewDataUtil.getWonderfulReviews();
  }

  // 设置用户评分
  setUserRating(rating: number): void {
    this.userRating = Math.max(1, Math.min(5, rating)); // 确保评分在1-5之间
  }

  // 切换筛选类型
  toggleFilter(filterType: ReviewFilterType): void {
    this.currentFilter = filterType;
  }

  // 筛选评论 - 返回筛选后的评论列表
  getFilteredReviews(): ReviewInfo[] {
    const reviews = this.getAllReviews();
    
    // 根据筛选类型处理评论列表
    if (this.currentFilter === ReviewFilterType.LATEST) {
      // 按时间排序
      return reviews.sort((a: ReviewInfo, b: ReviewInfo) => 
        new Date(b.createTime).getTime() - new Date(a.createTime).getTime()
      );
    } else {
      // 显示全部，按评分排序
      return reviews.sort((a: ReviewInfo, b: ReviewInfo) => b.rating - a.rating);
    }
  }

  // 跳转到阅读页面
  goToReading(): void {
    // 这里添加跳转到阅读页面的逻辑
    console.info('跳转到阅读页面');
  }
} 