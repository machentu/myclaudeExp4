import { BookBigCard, BookInfo, Constants, NavHeaderBar, TCRouter } from 'common';
import { SearchResultCard } from '../comps/HotRankCard';
import { LazyDataVM } from 'swiper_card'
export interface CategoryInfo {
  category: ResourceStr;
  books: BookInfo[]
}

/**
 * 书纵卡
 */
@ComponentV2
export struct BookViewListPage {
  @Local books: LazyDataVM<BookInfo> | undefined;
  @Local category: ResourceStr | undefined = '';

  async aboutToAppear(): Promise<void> {
    const p = TCRouter.getParams<CategoryInfo>(Constants.BOOK_LIST_ROUTE);
    this.category = p?.category;
    this.books = new LazyDataVM<BookInfo>(p?.books);
  }

  build() {
    NavDestination() {
      Stack({ alignContent: Alignment.TopStart }) {
        Column() {
          NavHeaderBar({
            title: this.category,
            hasBackButton: true,
            hasBgColor:  false,
            isMainPage: false,
          })
          Column(){
            List({ space: 10 }) {
              LazyForEach(this.books, (item: BookInfo) => {
                ListItem() {
                  BookBigCard({ info: item })
                }
              }, (item: string) => item)
            }
            .scrollBar(BarState.Off).cachedCount(3).edgeEffect(EdgeEffect.Spring)
          }

          .layoutWeight(1)
        }
      }
      .width(Constants.FULL_SIZE)
      .height(Constants.FULL_SIZE)
    }
    .hideTitleBar(true)
  }
}