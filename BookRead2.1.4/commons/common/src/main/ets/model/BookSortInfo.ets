export interface BookCategory {
  categoryId: string,
  categoryName: string,
  subcategory: BookSubcategory[]
}

export interface BookSubcategory {
  categoryId:string,
  subcategoryId:string,
  subcategoryName:string,
  imgUrl?:PixelMap | ResourceStr | DrawableDescriptor,
}