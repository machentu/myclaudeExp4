import {
  Constants,
  NavHeaderBar,
  TCRouter,
  BookInfo,
  BreakpointStorage,
  BreakpointSystem,
  BreakpointTypeEnum
} from 'common';
import { getRatingDescription } from '../model/ReviewModels';
import { ReviewDataUtil } from '../utils/ReviewDataUtil';
import { emitter } from '@kit.BasicServicesKit';

interface RouteParams {
  bookInfo?: BookInfo;
}

@ComponentV2
export struct WriteCommentPage {
  @Local currentRating: number = 0;
  @Local commentText: string = '';
  @Local textCount: number = 0;
  @Local currentBookInfo: BookInfo | undefined = undefined;
  @Local curBreakpoint: BreakpointStorage = BreakpointSystem.currentBreakpoint
  private maxLength: number = 500;

  aboutToAppear(): void {
    // 初始化评论系统，确保用户信息同步
    ReviewDataUtil.initialize();

    // 获取路由参数中的书籍信息
    const params = TCRouter.getParams<RouteParams>(Constants.WRITE_COMMENT_ROUTE);
    if (params && params.bookInfo) {
      this.currentBookInfo = params.bookInfo;
      console.info(`从路由参数获取书籍信息: ${params.bookInfo.name}`);
    } else {
      // 如果路由参数中没有书籍信息，使用ReviewDataUtil的辅助方法获取
      this.currentBookInfo = ReviewDataUtil.getCurrentBookInfo();
      if (this.currentBookInfo) {
        console.info(`从AppStorage获取书籍信息: ${this.currentBookInfo.name}`);
      } else {
        console.warn('未能获取到有效的书籍信息');
      }
    }

    try {
      // 监听用户信息变更事件
      emitter.on(Constants.LOGIN_CHANGE, () => {
        console.info('用户信息变更，将使用最新的用户信息提交评论');
      });
    } catch (e) {
      console.error('Failed to emitter on login change. Cause: ' + JSON.stringify(e));
    }
  }

  // 构建可点击的星级评分组件
  @Builder
  buildInteractiveStarRating() {
    Column() {
      Row({ space: 8 }) {
        ForEach(Array.from<number, number>({ length: 5 }, (_, index) => index + 1), (star: number) => {
          Image($r('app.media.ic_book_star'))
            .width(32)
            .height(32)
            .fillColor(star <= this.currentRating ? $r('sys.color.multi_color_11'):$r('sys.color.comp_background_secondary'))
            .onClick(() => {
              this.currentRating = star;
            })
        }, (item: string) => JSON.stringify(item))
      }
      .margin({ bottom: 16 })

      Text(getRatingDescription(this.currentRating))
        .fontSize(16)
        .fontWeight(500)
        .fontColor($r('sys.color.ohos_id_color_text_primary'))
        .textAlign(TextAlign.Center)
    }
    .width('100%')
    .alignItems(HorizontalAlign.Center)
    .margin({ top: 32, bottom: 32 })
  }

  // 构建评论输入框
  @Builder
  buildCommentTextArea() {
    Column() {
      TextArea({
        placeholder: '请输入您的评论...',
        text: this.commentText
      })
        .width('100%')
        .height(216)
        .borderRadius(16)
        .fontSize(14)
        .fontColor($r('sys.color.ohos_id_color_text_primary'))
        .placeholderColor($r('sys.color.ohos_id_color_text_hint'))
        .margin({ left: 16, right: 16 })
        .padding(16)
        .onChange((value: string) => {
          // 移除字数限制，允许输入超过500字，但要正确更新状态
          this.commentText = value;
          this.textCount = value.length;
        })

      // 字数统计 - 超过500字时显示红色
      Row() {
        Text(`${this.textCount}/${this.maxLength}`)
          .fontSize(12)
          .fontColor(this.textCount > this.maxLength ? Color.Red : $r('sys.color.ohos_id_color_text_hint'))
      }
      .width('100%')
      .justifyContent(FlexAlign.End)
      .padding({ right: 32 })
      .margin({ top: 8 })

      // 超过字数限制时的提示
      if (this.textCount > this.maxLength) {
        Row() {
          Text(`字数超过限制，请删除${this.textCount - this.maxLength}个字符`)
            .fontSize(12)
            .fontColor(Color.Red)
        }
        .width('100%')
        .justifyContent(FlexAlign.End)
        .padding({ right: 32 })
        .margin({ top: 4 })
      }
    }
    .width('100%')
    .alignItems(HorizontalAlign.Center)
  }

  // 构建提交按钮
  @Builder
  buildSubmitButton() {
    Button('提交评论')
      .width(328)
      .height(48)
      .backgroundColor(this.canSubmit() ?
        $r('sys.color.ohos_id_color_emphasize') : $r('sys.color.ohos_id_color_emphasize_contrary_dark'))
      .fontColor($r('sys.color.font_on_primary'))
      .fontSize($r('sys.float.Body_L'))
      .fontWeight(500)
      .margin({ top: 40, left: 16, right: 16 })
      .enabled(this.canSubmit())
      .onClick(() => {
        this.submitComment();
      })
  }

  // 检查是否可以提交评论
  private canSubmit(): boolean {
    return this.currentRating > 0 &&
      this.commentText.trim().length > 0 &&
      this.commentText.trim().length <= this.maxLength;
  }

  // 提交评论
  private submitComment(): void {
    let promptAction = this.getUIContext().getPromptAction();
    // 检查评分
    if (this.currentRating <= 0) {
      promptAction.showToast({
        message: '请先选择评分！',
        duration: 2000
      });
      return;
    }

    // 检查评论内容
    if (this.commentText.trim().length <= 0) {
      promptAction.showToast({
        message: '请输入评论内容！',
        duration: 2000
      });
      return;
    }

    // 检查字数限制
    if (this.commentText.trim().length > this.maxLength) {
      promptAction.showToast({
        message: `评论字数不能超过${this.maxLength}字，当前${this.commentText.trim().length}字`,
        duration: 2000
      });
      return;
    }

    try {
      // 使用当前存储的书籍信息或尝试重新获取
      let bookInfo: BookInfo | undefined = this.currentBookInfo;
      if (!bookInfo) {
        bookInfo = ReviewDataUtil.getCurrentBookInfo();
      }

      // 调用addReview方法，传入书籍信息
      ReviewDataUtil.addReview(this.currentRating, this.commentText.trim(), bookInfo);

      promptAction.showToast({
        message: '评论提交成功！',
        duration: 2000
      });

      console.info(`评论提交成功: 评分=${this.currentRating}, 内容长度=${this.commentText.trim()
        .length}, 书籍=${bookInfo?.name || '未知书籍'}`);

      const bookName = bookInfo?.name || '当前书籍';

      if (this.commentText.trim().length > 40) {
        TCRouter.replace(Constants.WONDERFUL_REVIEW_ROUTE);
      } else {
        TCRouter.replace(Constants.ALL_REVIEW_ROUTE, { bookName: bookName });
      }
    } catch (error) {
      console.error('提交评论失败:', error);
      promptAction.showToast({
        message: '评论提交失败，请重试',
        duration: 2000
      });
    }
  }

  build() {
    NavDestination() {
      Column() {
        NavHeaderBar({
          title: '写评论',
          hasBackButton: true,
          hasBgColor: false,
          isMainPage: true,
        })

        Column() {
          this.buildInteractiveStarRating()

          this.buildCommentTextArea()

          this.buildSubmitButton()
        }
        .width(this.curBreakpoint.value === BreakpointTypeEnum.LG ? '70%' : '100%')
        .height('100%')

      }
      .width('100%')
      .height('100%')
      .padding({ left: 12, right: 12 })
    }
    .hideTitleBar(true)
  }
} 