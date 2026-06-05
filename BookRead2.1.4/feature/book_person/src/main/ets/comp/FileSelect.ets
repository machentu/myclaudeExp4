import { photoAccessHelper } from '@kit.MediaLibraryKit';
import { hilog } from '@kit.PerformanceAnalysisKit';


export async function fileSelect(preselectedUris: Array<string>): Promise<Array<string>> {

  // 设置打开picker的选项
  let photoSelectOptions = new photoAccessHelper.PhotoSelectOptions();
  photoSelectOptions.MIMEType = photoAccessHelper.PhotoViewMIMETypes.IMAGE_TYPE; // 类型
  photoSelectOptions.maxSelectNumber = 4;                                        // 数量
  photoSelectOptions.preselectedUris = preselectedUris                           // 预先的uri

  // 推荐参数
  let recommendationOptions: photoAccessHelper.RecommendationOptions = {
    recommendationType: photoAccessHelper.RecommendationType.QR_CODE // 配置枚举类型，不同的枚举值，推荐不同种类的图片
  };
  photoSelectOptions.recommendationOptions = recommendationOptions;// 将推荐参数赋值给 photoSelectOptions.recommendationOptions

  let photoPicker = new photoAccessHelper.PhotoViewPicker();                     // 创建picker

  try {
    let photoSelectResult = await photoPicker.select(photoSelectOptions);        // 拉起图片选择框
    if (photoSelectResult && photoSelectResult.photoUris && photoSelectResult.photoUris.length > 0) {
      let imgUris = photoSelectResult.photoUris;
      for (let imgUri of imgUris) {
        if (imgUri.indexOf('media/Photo')<0) {
          let uiContext: UIContext = AppStorage.get('uiContext') as UIContext;
          uiContext.getPromptAction().showToast({message: '请选择图片进行上传'});
          return [];
        }
      }
      return imgUris;
    } else {
      return [];
    }
  } catch (err) {
    hilog.error(0,'SelectedImage failed', JSON.stringify(err));
    return [];
  }
}
