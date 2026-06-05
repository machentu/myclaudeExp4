import { Logger } from '@hw-agconnect/ui-base';
import { ComponentContent, promptAction } from '@kit.ArkUI';
import { DialogParams } from '../model/Book';

async function getUIContext() {
  let uiContext: UIContext = AppStorage.get('uiContext') as UIContext;
  return uiContext;
}

/**
 * 全局弹窗
 */
export class DialogUtil {
  static uiContext: UIContext;
  static contentNode: ComponentContent<Object>;
  static options: Object;
  static isOpen: boolean = false;
  static dialogBuilder: Object;

  static async openDialog(dialogBuilder: WrappedBuilder<[DialogParams]>, params: DialogParams): Promise<void> {
    if (DialogUtil.isOpen) {
      Logger.info('openDialog: dialog is already open')
      return
    }

    DialogUtil.isOpen = true

    try {
      if (!DialogUtil.uiContext) {
        DialogUtil.uiContext = await getUIContext()
      }
      DialogUtil.contentNode = new ComponentContent(DialogUtil.uiContext, dialogBuilder, params)
      let options: promptAction.BaseDialogOptions = {
        alignment: DialogAlignment.Center,
        autoCancel: true,
        offset: {
          dx: 0, dy: 0,
        },
        transition: undefined,
        showInSubWindow: false,
        keyboardAvoidMode: KeyboardAvoidMode.NONE,
        onWillDismiss: (action: DismissDialogAction) => {
          if (action.reason === DismissReason.PRESS_BACK) {
            action.dismiss()
          }
        },
      }

      const promptAction = DialogUtil.uiContext.getPromptAction()
      promptAction.openCustomDialog(DialogUtil.contentNode, options)
    } catch (error) {
      console.error('PromptActionUtils', ` closeCustomDialog error = ${JSON.stringify(error)}`)
    }
  }

  static async close(): Promise<void> {
    DialogUtil.isOpen = false

    try {
      const promptAction = DialogUtil.uiContext.getPromptAction()
      promptAction.closeCustomDialog(DialogUtil.contentNode)
    } catch (error) {
      console.error('PromptActionUtils', ` closeCustomDialog error = ${JSON.stringify(error)}`)
    }
  }
}