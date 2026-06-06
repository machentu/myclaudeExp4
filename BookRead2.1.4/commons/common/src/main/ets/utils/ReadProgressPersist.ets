import { fileIo } from '@kit.CoreFileKit';
import { util } from '@kit.ArkTS';
import { GlobalContext } from '../comp/GlobalContext';
import { GroupBook } from '../model/Book';

interface SavedProgress {
  readProgress: string;
  resourceIndex: number;
  startDomPos: string;
}

/**
 * 阅读进度持久化工具
 * 将每本书的阅读进度保存到沙箱文件中，确保进程关闭后重启仍能恢复进度
 */
export class ReadProgressPersist {
  private static readonly FILE_NAME: string = 'read_progress.json';

  private static getFilePath(): string {
    return GlobalContext.getContext().filesDir + '/' + ReadProgressPersist.FILE_NAME;
  }

  private static loadAll(): Record<string, SavedProgress> {
    try {
      const filePath = ReadProgressPersist.getFilePath();
      if (!fileIo.accessSync(filePath)) {
        return new Object() as Record<string, SavedProgress>;
      }
      const file = fileIo.openSync(filePath, fileIo.OpenMode.READ_ONLY);
      const stat = fileIo.statSync(filePath);
      const buf = new ArrayBuffer(stat.size);
      fileIo.readSync(file.fd, buf);
      fileIo.closeSync(file);
      const textDecoder = util.TextDecoder.create('utf-8', { fatal: false, ignoreBOM: true });
      const text = textDecoder.decodeToString(new Uint8Array(buf), { stream: false });
      return JSON.parse(text) as Record<string, SavedProgress>;
    } catch (e) {
      console.error('ReadProgressPersist loadAll failed: ' + JSON.stringify(e));
      return new Object() as Record<string, SavedProgress>;
    }
  }

  private static saveAll(data: Record<string, SavedProgress>): void {
    try {
      const filePath = ReadProgressPersist.getFilePath();
      const text = JSON.stringify(data);
      const file = fileIo.openSync(filePath, fileIo.OpenMode.CREATE | fileIo.OpenMode.READ_WRITE);
      fileIo.writeSync(file.fd, text);
      fileIo.closeSync(file);
    } catch (e) {
      console.error('ReadProgressPersist saveAll failed: ' + JSON.stringify(e));
    }
  }

  /**
   * 保存单本书的阅读进度
   */
  static saveProgress(bookId: string, readProgress: string, resourceIndex: number, startDomPos: string): void {
    const all = ReadProgressPersist.loadAll();
    all[bookId] = { readProgress, resourceIndex, startDomPos };
    ReadProgressPersist.saveAll(all);
  }

  /**
   * 读取单本书的阅读进度
   */
  static loadProgress(bookId: string): SavedProgress | null {
    const all = ReadProgressPersist.loadAll();
    return all[bookId] || null;
  }

  /**
   * 将已保存的阅读进度应用到 GroupBook 数组中
   */
  static applyProgressToBooks(favBooks: GroupBook[]): void {
    const all = ReadProgressPersist.loadAll();
    for (const groupBook of favBooks) {
      if (groupBook.singleBook?.id) {
        const saved = all[groupBook.singleBook.id];
        if (saved?.readProgress) {
          groupBook.singleBook.readProgress = saved.readProgress;
        }
      }
    }
  }
}
