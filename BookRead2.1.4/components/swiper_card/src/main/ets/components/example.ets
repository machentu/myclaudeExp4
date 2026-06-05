import { BookInfo } from '../model/Book';
import { LazyDataVM } from '../model/LazyDataVM';
import { bookSingleSwiper } from './BookSingleSwiper';
import {TUISwiper} from './TUISwiper'
const bookSingleSwiperBuilder: WrappedBuilder<[BookInfo]> = wrapBuilder(bookSingleSwiper);

@ComponentV2
export struct BookSwiperCard {
  @Param @Require books: LazyDataVM<BookInfo>
  @Local imgHeight: number = 250
  @Local imgWidth: number = 150
  @Local builderList: WrappedBuilder<[BookInfo]>[] = []

  aboutToAppear(): void {
    this.builderList.push(bookSingleSwiperBuilder);
  }

  build() {
    Column() {
      TUISwiper({
        imgWidth: this.imgWidth,
        imgHeight: this.imgHeight,
        builderList: this.builderList,
        bookList: this.books,
        isLoop: true,
        onImageClick: (index: number) => {
        }
      })
    }
  }
}