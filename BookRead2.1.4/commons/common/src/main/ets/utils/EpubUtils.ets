import { Constants } from '../constant/Constants';
import { GlobalContext } from '../comp/GlobalContext';
import { FileUtils } from './FileUtils';
import { fileIo } from '@kit.CoreFileKit';
import { BookInfo } from '../model/Book';
import { TCLogger } from '../comp/TCLogger';

export class EpubUtils {
  public static readEpubFile(filename: string, outputPath: string): string {
    // 本地调试使用，从 rawfile 文件中获取 epub 文件
    const filePath = filename + Constants.EPUB_FILE_SUFFIX;
    try {
      const rawByte: Uint8Array = GlobalContext.getContext().resourceManager.getRawFileContentSync(filePath);
      const zipname: string = filename + Constants.ZIP_FILE_SUFFIX;
      const inputPath: string = outputPath + Constants.FILE_SPLIT + zipname;
      const unZipPath: string = outputPath + Constants.FILE_SPLIT + filename + Constants.FILE_SPLIT;
      // 将文件写入沙箱
      FileUtils.writeFile(inputPath, rawByte.buffer);
      // 解压文件
      FileUtils.unzipFile(inputPath, unZipPath);
      return unZipPath;
    } catch (error) {
      TCLogger.error(`readEpubFile failed for ${filePath}: ${error}`);
      return '';
    }
  }

  public static async loadEpubUnzipFileFromNetwork(book: BookInfo, outputPath: string): Promise<string> {
    // 下载文件
    if (!fileIo.accessSync(outputPath)) {
      fileIo.mkdir(outputPath);
    }

    const zipname: string = book.id + Constants.ZIP_FILE_SUFFIX;
    const inputPath: string = outputPath + Constants.FILE_SPLIT + zipname;
    const unZipPath: string = outputPath + Constants.FILE_SPLIT + book.id + Constants.FILE_SPLIT;
    if (fileIo.accessSync(unZipPath)) {
      book.isDownload = true;
      return unZipPath;
    } else {
      await FileUtils.downloadFile(book, inputPath);
      // 解压文件
      await FileUtils.unzipFile(inputPath, unZipPath);
    }
    return unZipPath
  }

  public static async loadLocalEpubFile(book: BookInfo, outputPath: string): Promise<string> {
    // 根据 epubUrl 动态读取对应的 epub 文件
    // epubUrl 格式: "book_1" -> 读取 "book_1.epub"
    const rawFileName = book.epubUrl + Constants.EPUB_FILE_SUFFIX;
    const epubPath: string = outputPath + Constants.FILE_SPLIT + book.id + Constants.EPUB_FILE_SUFFIX;

    TCLogger.info(`loadLocalEpubFile: book.id=${book.id}, book.name=${book.name}, epubUrl=${book.epubUrl}`);
    TCLogger.info(`loadLocalEpubFile: rawFileName=${rawFileName}, epubPath=${epubPath}`);

    // 确保输出目录存在
    if (!fileIo.accessSync(outputPath)) {
      fileIo.mkdirSync(outputPath);
    }

    // 如果沙箱中已存在该书籍的 epub，直接返回
    if (fileIo.accessSync(epubPath)) {
      TCLogger.info(`loadLocalEpubFile: epub already exists in sandbox at ${epubPath}`);
      book.isDownload = true;
      return epubPath;
    }

    // 尝试读取指定的 epub 文件
    try {
      TCLogger.info(`loadLocalEpubFile: attempting to read ${rawFileName} from rawfile`);
      const rawByte: Uint8Array = GlobalContext.getContext().resourceManager.getRawFileContentSync(rawFileName);
      TCLogger.info(`loadLocalEpubFile: successfully read ${rawFileName}, size=${rawByte.length}`);
      FileUtils.writeFile(epubPath, rawByte.buffer);
      TCLogger.info(`loadLocalEpubFile: wrote epub to ${epubPath}`);
      book.isDownload = true;
      return epubPath;
    } catch (error) {
      TCLogger.error(`loadLocalEpubFile failed for ${rawFileName}: ${error}`);

      // 如果读取失败，尝试使用 example.epub 作为备选
      try {
        TCLogger.info(`loadLocalEpubFile: falling back to example.epub`);
        const fallbackByte: Uint8Array = GlobalContext.getContext().resourceManager.getRawFileContentSync('example.epub');
        TCLogger.info(`loadLocalEpubFile: successfully read example.epub, size=${fallbackByte.length}`);
        FileUtils.writeFile(epubPath, fallbackByte.buffer);
        TCLogger.info(`loadLocalEpubFile: wrote fallback epub to ${epubPath}`);
        book.isDownload = true;
        return epubPath;
      } catch (fallbackError) {
        TCLogger.error(`loadLocalEpubFile fallback also failed: ${fallbackError}`);
        return epubPath;
      }
    }
  }

  public static async loadEpubFileFromNetwork(book: BookInfo, outputPath: string): Promise<string> {
    if (!fileIo.accessSync(outputPath)) {
      fileIo.mkdir(outputPath);
    }
    const epubPath: string = outputPath + Constants.FILE_SPLIT + book.id + Constants.EPUB_FILE_SUFFIX;
    if (fileIo.accessSync(epubPath)) {
      return epubPath
    } else {
      await FileUtils.downloadFile(book, epubPath);
    }
    return epubPath
  }

  public static clearEpubFile(book: BookInfo, outputPath: string, from: string){
    const epubPath: string = outputPath + Constants.FILE_SPLIT + book.id + Constants.EPUB_FILE_SUFFIX;
    if (fileIo.accessSync(epubPath) && outputPath.includes(from)) {
      fileIo.unlinkSync(epubPath);
    }
  }
}