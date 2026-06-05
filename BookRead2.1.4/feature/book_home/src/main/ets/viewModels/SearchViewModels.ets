
@ObservedV2
export  class SearchViewModels {
  @Trace historyInfo: string[] = ['小','斗破苍穹','诛仙','红楼梦','水浒传','钢铁','安娜的故事','名侦探柯南'];

  @Trace hotInfo: string[] = ['小羊肖恩','诛仙','旅途'];
  @Trace keyword: string = '';
  private static _instance: SearchViewModels;

  public static get instance() {
    if (!SearchViewModels._instance) {
      SearchViewModels._instance = new SearchViewModels();
    }
    return SearchViewModels._instance;
  }

  // 搜索
  public async searchDishes() {
    // 插入历史记录,并去重
    this.historyInfo.unshift(this.keyword);
    this.historyInfo = Array.from(new Set(this.historyInfo));
  }
}

