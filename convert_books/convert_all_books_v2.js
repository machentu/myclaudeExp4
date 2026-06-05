const fs = require('fs');
const path = require('path');
const archiver = require('archiver');
const iconv = require('iconv-lite');

const LITERATURE_DIR = path.join(__dirname, '../readtry/literature-books');
const OUTPUT_DIR = path.join(__dirname, '../BookRead2.1.4/entry/src/main/resources/rawfile');
const DATA_JSON_PATH = path.join(__dirname, '../BookRead2.1.4/entry/src/main/resources/rawfile/data.json');

// 22本书的配置 - 按顺序编号，直接输出到 rawfile 根目录
const BOOKS_CONFIG = [
  { file: '《狼图腾》全集.txt', id: '1', name: '狼图腾', author: '姜戎', epubFile: 'book_1.epub' },
  { file: '《白鹿原》全集.txt', id: '2', name: '白鹿原', author: '陈忠实', epubFile: 'book_2.epub' },
  { file: '万历十五年.txt', id: '3', name: '万历十五年', author: '黄仁宇', epubFile: 'book_3.epub' },
  { file: '三国演义.txt', id: '4', name: '三国演义', author: '罗贯中', epubFile: 'book_4.epub' },
  { file: '《全球通史》.txt', id: '5', name: '全球通史', author: '斯塔夫里阿诺斯', epubFile: 'book_5.epub' },
  { file: '人类简史.txt', id: '6', name: '人类简史', author: '尤瓦尔·赫拉利', epubFile: 'book_6.epub' },
  { file: '丑陋的中国人.txt', id: '7', name: '丑陋的中国人', author: '柏杨', epubFile: 'book_7.epub' },
  { file: '丰乳肥臀.txt', id: '8', name: '丰乳肥臀', author: '莫言', epubFile: 'book_8.epub' },
  { file: '《曾国藩全集》.txt', id: '9', name: '曾国藩全集', author: '曾国藩', epubFile: 'book_9.epub' },
  { file: '《毛泽东传》（罗斯.特里尔版）.txt', id: '10', name: '毛泽东传', author: '罗斯·特里尔', epubFile: 'book_10.epub' },
  { file: '《溥仪自传--我的前半生》.txt', id: '11', name: '我的前半生', author: '溥仪', epubFile: 'book_11.epub' },
  { file: '《血色浪漫》作者：都梁.txt', id: '12', name: '血色浪漫', author: '都梁', epubFile: 'book_12.epub' },
  { file: '《雍正王朝》.txt', id: '13', name: '雍正王朝', author: '二月河', epubFile: 'book_13.epub' },
  { file: '四世同堂.txt', id: '14', name: '四世同堂', author: '老舍', epubFile: 'book_14.epub' },
  { file: '《大明王朝的七张面孔》(Ⅰ+Ⅱ终结篇)（完结）作者：张宏杰.txt', id: '15', name: '大明王朝的七张面孔', author: '张宏杰', epubFile: 'book_15.epub' },
  { file: '《历史深处的忧虑》TXT下载（全本）作者：林达.txt', id: '16', name: '历史深处的忧虑', author: '林达', epubFile: 'book_16.epub' },
  { file: '《常识》梁文道.txt', id: '17', name: '常识', author: '梁文道', epubFile: 'book_17.epub' },
  { file: '俗世奇人.txt', id: '18', name: '俗世奇人', author: '冯骥才', epubFile: 'book_18.epub' },
  { file: '万水千山走遍.txt', id: '19', name: '万水千山走遍', author: '三毛', epubFile: 'book_19.epub' },
  { file: '三毛-梦里花落知多少.txt', id: '20', name: '梦里花落知多少', author: '三毛', epubFile: 'book_20.epub' },
  { file: '撒哈拉的故事.txt', id: '21', name: '撒哈拉的故事', author: '三毛', epubFile: 'book_21.epub' },
  { file: '《唤醒心中的巨人》.txt', id: '22', name: '唤醒心中的巨人', author: '安东尼·罗宾', epubFile: 'book_22.epub' }
];

function readFileContent(filePath) {
  const buffer = fs.readFileSync(filePath);

  // Try UTF-8 first (most common for modern files)
  try {
    const utf8Content = buffer.toString('utf8');
    // Check for replacement character which indicates decoding failure
    if (utf8Content.indexOf('�') === -1) {
      console.log(`  Encoding: UTF-8`);
      return utf8Content;
    }
  } catch (e) {}

  // Try GBK/GB2312 (common Chinese encoding)
  try {
    const gbkContent = iconv.decode(buffer, 'gbk');
    if (gbkContent.indexOf('�') === -1) {
      console.log(`  Encoding: GBK`);
      return gbkContent;
    }
  } catch (e) {}

  // Fallback to UTF-8
  console.log(`  Encoding: UTF-8 (fallback)`);
  return buffer.toString('utf8');
}

function splitChapters(content) {
  const chapters = [];
  const patterns = [
    /第[一二三四五六七八九十百千万零\d]+[章节回部卷集][^\n]*/g
  ];
  let matches = [];
  for (const pattern of patterns) {
    const found = [...content.matchAll(pattern)];
    if (found.length > 3) { matches = found; break; }
  }

  if (matches.length > 3) {
    for (let i = 0; i < matches.length; i++) {
      const start = matches[i].index;
      const end = i < matches.length - 1 ? matches[i + 1].index : content.length;
      const chapterContent = content.slice(start, end).trim();
      const chapterTitle = matches[i][0].trim();
      if (chapterContent.length > 200) {
        chapters.push({ title: chapterTitle, content: chapterContent });
      }
    }
  }

  if (chapters.length === 0) {
    const lines = content.split('\n').filter(line => line.trim().length > 10);
    for (let i = 0; i < lines.length; i += 100) {
      const chunk = lines.slice(i, i + 100);
      if (chunk.length > 0) {
        chapters.push({ title: `第${Math.floor(i / 100) + 1}节`, content: chunk.join('\n') });
      }
    }
  }
  return chapters;
}

function createEpub(bookId, bookName, author, chapters, outputPath) {
  return new Promise((resolve, reject) => {
    const output = fs.createWriteStream(outputPath);
    const archive = archiver('zip', { zlib: { level: 9 } });
    output.on('close', () => resolve(outputPath));
    archive.on('error', reject);
    archive.pipe(output);

    archive.append('application/epub+zip', { name: 'mimetype', store: true });

    archive.append(`<?xml version="1.0" encoding="UTF-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles>
     <rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>
  </rootfiles>
</container>`, { name: 'META-INF/container.xml' });

    const cssContent = `.bodyContent-1 { margin: 0.5em 0; text-indent: 2em; font-family: serif; line-height: 1.5; }
.chaptertitle-c { text-align: center; margin: 1em 0; font-size: 1.2em; }`;
    archive.append(cssContent, { name: 'OEBPS/flow0001.css' });

    const manifestItems = [];
    const spineItems = [];
    const navPoints = [];
    const uuid = `${bookId}-${Date.now()}`;

    chapters.forEach((chapter, index) => {
      const fileName = `text${String(index + 10).padStart(5, '0')}.html`;
      const idNum = index + 10;

      const paragraphs = chapter.content.split('\n')
        .filter(line => line.trim().length > 0)
        .map(line => `<p class="bodyContent-1">${line.trim().replace(/</g, '&lt;').replace(/&/g, '&amp;')}</p>`)
        .join('\n');

      const htmlContent = `<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="zh-Hans" lang="zh-Hans">
<head>
<title>${chapter.title.replace(/</g, '&lt;')}</title>
<link href="flow0001.css" rel="stylesheet" type="text/css" />
</head>
<body>
<h1 class="chaptertitle-c">${chapter.title.replace(/</g, '&lt;')}</h1>
${paragraphs}
</body>
</html>`;

      archive.append(htmlContent, { name: `OEBPS/${fileName}` });

      manifestItems.push(`<item href="${fileName}" id="id_${idNum}" media-type="application/xhtml+xml"/>`);
      spineItems.push(`<itemref idref="id_${idNum}"/>`);
      navPoints.push(`<navPoint id="navpoint_${index + 1}" playOrder="${index + 1}">
<navLabel><text>${chapter.title.replace(/</g, '&lt;')}</text></navLabel>
<content src="${fileName}"/></navPoint>`);
    });

    const bookNameSafe = bookName.replace(/</g, '&lt;').replace(/&/g, '&amp;');
    const authorSafe = author.replace(/</g, '&lt;').replace(/&/g, '&amp;');

    const contentOpf = `<?xml version='1.0' encoding='utf-8'?>
<package xmlns:opf="http://www.idpf.org/2007/opf" xmlns="http://www.idpf.org/2007/opf" unique-identifier="bookid" version="2.0">
  <metadata xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:opf="http://www.idpf.org/2007/opf" xmlns:dcterms="http://purl.org/dc/terms/" xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:title>${bookNameSafe}</dc:title>
    <dc:creator opf:role="aut">${authorSafe}</dc:creator>
    <dc:identifier id="bookid">urn:uuid:${uuid}</dc:identifier>
    <dc:language>zh</dc:language>
  </metadata>
  <manifest>
    ${manifestItems.join('\n    ')}
    <item href="toc.ncx" id="ncx" media-type="application/x-dtbncx+xml"/>
    <item href="flow0001.css" id="css1" media-type="text/css"/>
  </manifest>
  <spine toc="ncx">
    ${spineItems.join('\n    ')}
  </spine>
  <guide></guide>
</package>`;
    archive.append(contentOpf, { name: 'OEBPS/content.opf' });

    const tocNcx = `<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE ncx PUBLIC "-//NISO//DTD ncx 2005-1//EN" "http://www.daisy.org/z3986/2005/ncx-2005-1.dtd">
<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">
<head>
<meta name="dtb:uid" content="urn:uuid:${uuid}"/>
<meta name="dtb:depth" content="1"/>
<meta name="dtb:totalPageCount" content="0"/>
<meta name="dtb:maxPageNumber" content="0"/>
</head>
<docTitle><text>${bookNameSafe}</text></docTitle>
<navMap>
${navPoints.join('\n')}
</navMap>
</ncx>`;
    archive.append(tocNcx, { name: 'OEBPS/toc.ncx' });

    archive.finalize();
  });
}

async function main() {
  console.log('Converting 22 books to EPUB (version 2 - correct mapping)...\n');

  // 先清理旧的 book_*.epub 文件（保留 book_100.epub 和 example.epub）
  const existingFiles = fs.readdirSync(OUTPUT_DIR).filter(f =>
    f.startsWith('book_') && f.endsWith('.epub') && f !== 'book_100.epub'
  );
  existingFiles.forEach(f => {
    try {
      fs.unlinkSync(path.join(OUTPUT_DIR, f));
      console.log(`Removed old file: ${f}`);
    } catch (e) {}
  });

  const convertedBooks = [];
  const timestamp = Date.now();

  for (const config of BOOKS_CONFIG) {
    const txtPath = path.join(LITERATURE_DIR, config.file);

    if (!fs.existsSync(txtPath)) {
      console.log(`Warning: ${config.file} not found, skipping...`);
      continue;
    }

    console.log(`Processing [${config.id}]: ${config.name} (${config.author})`);

    try {
      const content = readFileContent(txtPath);
      const chapters = splitChapters(content);
      console.log(`  Chapters: ${chapters.length}`);

      const outputPath = path.join(OUTPUT_DIR, config.epubFile);

      await createEpub(config.id, config.name, config.author, chapters, outputPath);
      console.log(`  Created: ${config.epubFile}`);

      const bookEntry = {
        "id": config.id,
        "name": config.name,
        "author": config.author,
        "coverUrl": "app.media.book_image_1",
        "category": "文学",
        "rate": "8",
        "epubUrl": config.epubFile.replace('.epub', ''),
        "description": `${config.name} - ${config.author}的作品`,
        "count": chapters.length.toString(),
        "popular": "10.5",
        "isFree": "0",
        "gender": "1",
        "groupName": "",
        "groupType": "0",
        "status": "1",
        "readProgress": "未读过"
      };

      convertedBooks.push(bookEntry);

    } catch (error) {
      console.log(`  Error: ${error.message}`);
    }
  }

  // Create data.json
  const data = {
    "/book/list": { "books": convertedBooks },
    "/book/favourite": { "books": convertedBooks },
    "/book/group": {
      "books": convertedBooks.map((book, index) => ({
        "bookName": book.name,
        "isSelect": false,
        "groupName": "",
        "singleBook": book,
        "timestamp": timestamp + index,
        "isDownload": false,
        "downloading": false,
        "isPin": false
      }))
    },
    "/book/borrow": { "borrows": [] },
    "/book/hotRankList": { "rankList": [] },
    "/user/info": {
      "user": {
        "id": "1",
        "nickName": "读者",
        "birthday": "2000-01-01",
        "telephone": "",
        "borrowCardStart": 1731340800000,
        "borrowCardEnd": 1762876800000,
        "totalReading": 0,
        "totalListening": 0
      }
    },
    "/book/history": { "books": [] },
    "/book/categoryList": { "category": [] },
    "/book/recommend": { "books": convertedBooks.slice(0, 5) },
    "/book/like": { "books": [] },
    "/book/child": { "books": [] }
  };

  fs.writeFileSync(DATA_JSON_PATH, JSON.stringify(data, null, 2));
  console.log(`\nCompleted! Converted ${convertedBooks.length} books.`);
  console.log(`Updated data.json at: ${DATA_JSON_PATH}`);
  console.log('\nMapping:');
  convertedBooks.forEach(b => console.log(`  ${b.id}: ${b.name} -> ${b.epubUrl}`));
}

main().catch(console.error);