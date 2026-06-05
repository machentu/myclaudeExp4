import { BookInfo } from '../model/Book';

@Builder
export function bookSingleSwiper(info?: BookInfo) {
  Column() {
    Image($r(info?.coverUrl?.toString()))
      .width('100%')
      .height('100%')
      .borderRadius(10)
  }
}