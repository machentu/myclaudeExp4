import { Constants, NavHeaderBar, TCRouter, BookInfo } from 'common';
import { ReviewInfo } from '../model/ReviewModels';
import { ReviewDataUtil } from '../utils/ReviewDataUtil';
import { ExpandableText } from '../components/ExpandableText';
import { emitter } from '@kit.BasicServicesKit';

interface RouteParams {
  bookName?: string;
  fromCompleted?: boolean;
}

@ComponentV2
export struct WonderfulReviewPage {
  @Local reviewList: ReviewInfo[] = [];
  @Local isFromCompleted: boolean = true;
  bottomRectHeight: number = AppStorage.get('bottomRectHeight') as number || 38.77

  aboutToAppear(): void {
    // 初始化评论系统，确保用户信息同步
    ReviewDataUtil.initialize();

    // 获取路由参数，判断是否从已读完状态进入
    const params = TCRouter.getParams<RouteParams>(Constants.WONDERFUL_REVIEW_ROUTE);
    if (params && params.fromCompleted !== undefined) {
      this.isFromCompleted = params.fromCompleted;
      console.info(`精彩书评页面来源: ${this.isFromCompleted ? '最后一页左滑(已读完)' : '首页更多评论(未读完)'}`);
    }

    this.loadReviews();

    try {
      // 监听用户信息变更事件
      emitter.on(Constants.LOGIN_CHANGE, () => {
        console.info('用户信息变更，更新评论用户信息');
        ReviewDataUtil.updateCurrentUserReviews();
        this.loadReviews();
      });
    } catch (e) {
      console.error('Failed to emitter on login change. Cause: ' + JSON.stringify(e));
    }

  }

  onPageShow(): void {
    this.loadReviews();
  }

  loadReviews(): void {
    this.reviewList = ReviewDataUtil.getWonderfulReviews();
  }

  // 构建阅读状态框
  @Builder
  buildCompletedStatusCard() {
    Column() {
      Text(this.isFromCompleted ? $r('app.string.finished_reading') : $r('app.string.no_finished_reading'))
        .fontSize(20)
        .lineHeight(27)
        .fontWeight(500)
        .fontColor($r('sys.color.font_secondary'))
        .opacity(0.9)
        .margin({ bottom: 16 })

      Row({ space: 4 }) {
        ForEach(Array.from<number, number>({ length: 5 }, (_, index) => index + 1), (star: number) => {
          Image($r('app.media.ic_book_star'))
            .width(20)
            .height(20)
            .fillColor($r('sys.color.comp_background_secondary'))
        }, (item: string) => JSON.stringify(item))
      }
      .margin({ bottom: 16 })

      Row() {
        Image(this.isFromCompleted ?
          $r('app.media.ic_public_lock_open_base_ic_public_lock_open') :
          $r('app.media.ic_public_locked'))
          .width(16)
          .height(16)
          .margin({ right: 8 })

        Text('去评价')
          .fontSize(14)
          .lineHeight(19)
          .fontWeight(500)
          .fontColor($r('sys.color.font_secondary'))
          .opacity(0.9)

        Image($r('app.media.arrow_right_small'))
          .width(12)
          .height(24)
          .margin({ left: 8 })
      }
      .alignItems(VerticalAlign.Center)
      .onClick(() => {
        // 如果是未读完状态，阻止跳转并显示提示
        if (!this.isFromCompleted) {
          this.getUIContext().getPromptAction().showToast({
            message: '看完才可以评论哦，快去看书吧！',
            duration: 2000
          });
          console.info('未读完状态，阻止跳转到评论页面');
          return;
        }

        // 已读完状态，正常跳转到评论页面
        const currentBook = ReviewDataUtil.getCurrentBookInfo();
        if (currentBook) {
          console.info(`从精彩书评页面跳转到写评论页面，书籍: ${currentBook.name}`);
          TCRouter.push(Constants.WRITE_COMMENT_ROUTE, { bookInfo: currentBook });
        } else {
          console.warn('未找到当前书籍信息，仍然跳转到写评论页面');
          TCRouter.push(Constants.WRITE_COMMENT_ROUTE);
        }
      })
    }
    .width('100%')
    .height(127)
    .backgroundColor($r('sys.color.comp_background_list_card'))
    .borderRadius(16)
    .justifyContent(FlexAlign.Center)
    .alignItems(HorizontalAlign.Center)
    .margin({ top: 20 })
  }

  // 构建星级评分组件
  @Builder
  buildStarRating(rating: number) {
    Row() {
      ForEach(Array.from<number, number>({ length: 5 }, (_, index) => index + 1), (star: number) => {
        Image($r('app.media.ic_public_star'))
          .width(12)
          .height(12)
          .margin({ right: 2 })
          .fillColor(star <= rating ? '#FFD700' : '#E0E0E0')
      }, (item: string) => JSON.stringify(item))
    }
  }

  // 构建单个评论项
  @Builder
  buildReviewItem(review: ReviewInfo, index: number) {
    Column() {
      Row() {
        Image(review.avatar)
          .width(40)
          .height(40)
          .borderRadius(20)
          .margin({ right: 12 })

        Column() {
          Row() {
            Text(review.username)
              .fontSize(14)
              .fontWeight(FontWeight.Medium)
              .fontColor($r('sys.color.ohos_id_color_text_primary'))

            if (review.isVip && review.vipLevel) {
              Text(review.vipLevel)
                .fontSize(10)
                .fontColor(Color.White)
                .padding({
                  left: 6,
                  right: 6,
                  top: 2,
                  bottom: 2
                })
                .backgroundImage($r('app.media.lv_bg'))
                .backgroundImageSize(ImageSize.Contain)
                .margin({ left: 5 })
            }
          }
          .width('100%')
          .alignItems(VerticalAlign.Center)

          this.buildStarRating(review.rating)
        }
        .layoutWeight(1)
        .alignItems(HorizontalAlign.Start)
      }
      .width('100%')
      .alignItems(VerticalAlign.Top)
      .margin({ bottom: 12 })

      ExpandableText({
        content: review.content.toString(),
        fontSize: 14,
        lineHeight: 20,
        fontColor: $r('sys.color.ohos_id_color_text_primary'),
        widthRatio: 0.8
      })
        .margin({ bottom: 8 })

      // 分割线
      if (index < this.reviewList.length - 1) {
        Divider()
          .margin({ bottom: 16 })
      } else {
        // 最后一项增加额外的底部间距
        Blank()
          .height(20)
      }
    }
    .width('100%')
    .margin({ top: 5 })
    .alignItems(HorizontalAlign.Start)
  }

  // 构建精彩书评列表
  @Builder
  buildReviewListSection() {
    Column() {
      // 标题栏
      Row() {
        Text('精彩书评')
          .fontSize(16)
          .lineHeight(21)
          .fontWeight(500)
          .fontColor($r('sys.color.ohos_id_color_text_primary'))
          .opacity(0.9)

        Row() {
          Text('查看全部')
            .fontSize(12)
            .lineHeight(16)
            .fontWeight(400)
            .fontColor($r('sys.color.ohos_id_color_text_hint'))
            .opacity(0.6)
            .textAlign(TextAlign.End)

          Image($r('app.media.ic_public_right_arrow'))
            .width(16)
            .height(16)
            .margin({ left: 3 })
            .opacity(0.6)
        }
        .onClick(() => {
          // 获取当前书籍信息
          const currentBook = ReviewDataUtil.getCurrentBookInfo();
          const bookName = currentBook?.name || '当前书籍';
          TCRouter.push(Constants.ALL_REVIEW_ROUTE, { bookName: bookName });
        })
      }
      .width('100%')
      .justifyContent(FlexAlign.SpaceBetween)
      .alignItems(VerticalAlign.Center)
      .margin({ top: 20, bottom: 16 })

      Column({ space: 0 }) {
        ForEach(this.reviewList, (review: ReviewInfo, index: number) => {
          this.buildReviewItem(review, index);
        }, (review: ReviewInfo) => review.id)
      }
    }
    .width('100%')
    .padding({ left: 16, right: 16 })
    .backgroundColor($r('sys.color.comp_background_list_card'))
    .alignItems(HorizontalAlign.Start)
    .margin({
      top: 5
    })
  }

  build() {
    NavDestination() {
      Column() {
        NavHeaderBar({
          title: '精彩书评',
          hasBackButton: true,
          hasBgColor: false,
          isMainPage: true,
        })

        Column() {
          // 已读完状态卡片
          this.buildCompletedStatusCard()

          // 精彩书评列表
          Scroll() {
            this.buildReviewListSection()
          }
          .layoutWeight(1)
          .scrollable(ScrollDirection.Vertical)
          .edgeEffect(EdgeEffect.Spring)
          .scrollBar(BarState.Off)
          .margin({ top: 10 })
          .borderRadius(16)
        }
        .width('100%')
        .layoutWeight(1)
        .padding({ left: 12, right: 12, bottom: this.bottomRectHeight })
      }
      .width('100%')
      .height('100%')
      .backgroundColor($r('sys.color.ohos_id_color_sub_background'))
    }
    .hideTitleBar(true)
  }
} 