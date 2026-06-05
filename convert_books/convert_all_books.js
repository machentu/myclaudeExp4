const fs = require('fs');
const path = require('path');
const archiver = require('archiver');
const iconv = require('iconv-lite');

const LITERATURE_DIR = path.join(__dirname, '../readtry/literature-books');
const OUTPUT_DIR = path.join(__dirname, '../BookRead2.1.4/entry/src/main/resources/rawfile');
const BOOKS_OUTPUT_DIR = path.join(OUTPUT_DIR, 'books');
const DATA_JSON_PATH = path.join(__dirname, '../BookRead2.1.4/entry/src/main/resources/rawfile/data.json');

// 22本书的配置 - 从literature-books中选取
const BOOKS_CONFIG = [
  { file: '《狼图腾》全集.txt', name: '狼图腾', author: '姜戎', description: '《狼图腾》是一部描绘蒙古草原狼群与人类生存斗争的史诗性小说，展现了草原民族的狼图腾崇拜和生态智慧。' },
  { file: '《白鹿原》全集.txt', name: '白鹿原', author: '陈忠实', description: '《白鹿原》是一部渭河平原50年变迁的雄奇史诗，一轴中国农村斑斓多彩、触目惊心的长幅画卷。' },
  { file: '万历十五年.txt', name: '万历十五年', author: '黄仁宇', description: '《万历十五年》是黄仁宇的成名之作，以1587年为切入点，剖析明朝政治体制的深层问题。' },
  { file: '三国演义.txt', name: '三国演义', author: '罗贯中', description: '《三国演义》是中国古典四大名著之一，描绘了东汉末年至西晋初年的历史风云。' },
  { file: '《全球通史》.txt', name: '全球通史', author: '斯塔夫里阿诺斯', description: '《全球通史》是一部从全球视角审视人类历史的经典著作。' },
  { file: '人类简史.txt', name: '人类简史', author: '尤瓦尔·赫拉利', description: '《人类简史》从认知革命、农业革命到科学革命，讲述了人类如何登上食物链顶端。' },
  { file: '丑陋的中国人.txt', name: '丑陋的中国人', author: '柏杨', description: '《丑陋的中国人》是柏杨先生的经典著作，深刻剖析了中国传统文化的负面因素。' },
  { file: '丰乳肥臀.txt', name: '丰乳肥臀', author: '莫言', description: '《丰乳肥臀》是莫言的长篇小说，讲述了一位母亲和她的八个女儿的故事。' },
  { file: '《曾国藩全集》.txt', name: '曾国藩全集', author: '曾国藩', description: '《曾国藩全集》收录了晚清名臣曾国藩的文章、书信和日记。' },
  { file: '《毛泽东传》（罗斯.特里尔版）.txt', name: '毛泽东传', author: '罗斯·特里尔', description: '《毛泽东传》是美国学者罗斯·特里尔撰写的毛泽东传记。' },
  { file: '《溥仪自传--我的前半生》.txt', name: '我的前半生', author: '溥仪', description: '《我的前半生》是中国末代皇帝爱新觉罗·溥仪的自传。' },
  { file: '《血色浪漫》作者：都梁.txt', name: '血色浪漫', author: '都梁', description: '《血色浪漫》描写了文革时期北京青年的生活状态和情感世界。' },
  { file: '《雍正王朝》.txt', name: '雍正王朝', author: '二月河', description: '《雍正王朝》是二月河的历史小说，描绘雍正皇帝的政治生涯。' },
  { file: '四世同堂.txt', name: '四世同堂', author: '老舍', description: '《四世同堂》是老舍的代表作，描绘了抗战时期北平小羊圈胡同的生活。' },
  { file: '《大明王朝的七张面孔》(Ⅰ+Ⅱ终结篇)（完结）作者：张宏杰.txt', name: '大明王朝的七张面孔', author: '张宏杰', description: '《大明王朝的七张面孔》通过七个典型人物解读明朝历史。' },
  { file: '《历史深处的忧虑》TXT下载（全本）作者：林达.txt', name: '历史深处的忧虑', author: '林达', description: '《历史深处的忧虑》通过观察美国社会制度，探讨民主与法治。' },
  { file: '《常识》梁文道.txt', name: '常识', author: '梁文道', description: '《常识》是梁文道的时评文集，探讨当代社会现象。' },
  { file: '俗世奇人.txt', name: '俗世奇人', author: '冯骥才', description: '《俗世奇人》描写天津卫市井生活中的奇人异事。' },
  { file: '万水千山走遍.txt', name: '万水千山走遍', author: '三毛', description: '《万水千山走遍》是三毛的游记作品，记录了她的中南美洲之旅。' },
  { file: '三毛-梦里花落知多少.txt', name: '梦里花落知多少', author: '三毛', description: '《梦里花落知多少》是三毛悼念丈夫荷西的散文集。' },
  { file: '撒哈拉的故事.txt', name: '撒哈拉的故事', author: '三毛', description: '《撒哈拉的故事》记录了三毛在撒哈拉沙漠的生活经历。' },
  { file: '《唤醒心中的巨人》.txt', name: '唤醒心中的巨人', author: '安东尼·罗宾', description: '《唤醒心中的巨人》是潜能开发专家安东尼·罗宾的代表作。' }
];

function readFileContent(filePath) {
  const buffer = fs.readFileSync(filePath);
  try {
    const utf8Content = buffer.toString('utf8');
    if (utf8Content.indexOf('�') === -1) return utf8Content;
  } catch (e) {}
  try { return iconv.decode(buffer, 'gbk'); } catch (e) {}
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

function createEpubLikeExample(bookId, bookName, author, chapters, outputPath) {
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
  console.log('Converting 22 books to EPUB...\n');

  // Ensure books output directory exists
  if (!fs.existsSync(BOOKS_OUTPUT_DIR)) {
    fs.mkdirSync(BOOKS_OUTPUT_DIR, { recursive: true });
  }

  const convertedBooks = [];
  const timestamp = Date.now();

  for (let i = 0; i < BOOKS_CONFIG.length; i++) {
    const config = BOOKS_CONFIG[i];
    const txtPath = path.join(LITERATURE_DIR, config.file);

    if (!fs.existsSync(txtPath)) {
      console.log(`Warning: ${config.file} not found, skipping...`);
      continue;
    }

    console.log(`Processing: ${config.name} (${config.author})`);

    try {
      const content = readFileContent(txtPath);
      const chapters = splitChapters(content);
      console.log(`  Chapters: ${chapters.length}`);

      const epubFileName = `${config.name}.epub`;
      const outputPath = path.join(BOOKS_OUTPUT_DIR, epubFileName);

      await createEpubLikeExample(String(i + 1), config.name, config.author, chapters, outputPath);
      console.log(`  Created: books/${epubFileName}`);

      const bookEntry = {
        "id": String(i + 1),
        "name": config.name,
        "author": config.author,
        "coverUrl": "app.media.book_image_1",
        "category": "文学",
        "rate": "8",
        "epubUrl": `books/${config.name}`,
        "description": config.description,
        "count": String(chapters.length),
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
}

main().catch(console.error);