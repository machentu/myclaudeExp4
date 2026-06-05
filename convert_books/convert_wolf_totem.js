const fs = require('fs');
const path = require('path');
const archiver = require('archiver');
const iconv = require('iconv-lite');

const TXT_PATH = path.join(__dirname, '《白鹿原》全集.txt');
const OUTPUT_DIR = path.join(__dirname, '../BookRead2.1.4/entry/src/main/resources/rawfile');
const DATA_JSON_PATH = path.join(__dirname, '../BookRead2.1.4/entry/src/main/resources/rawfile/data.json');

function readFileContent(filePath) {
  const buffer = fs.readFileSync(filePath);
  try {
    const utf8Content = buffer.toString('utf8');
    if (utf8Content.indexOf('\uFFFD') === -1) return utf8Content;
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
  console.log('Converting 白鹿原 to EPUB...\n');

  const content = readFileContent(TXT_PATH);
  const chapters = splitChapters(content);

  const bookName = '白鹿原';
  const author = '陈忠实';
  const outputPath = path.join(OUTPUT_DIR, '白鹿原.epub');

  console.log(`Book: ${bookName}`);
  console.log(`Author: ${author}`);
  console.log(`Chapters: ${chapters.length}`);
  console.log(`Output: ${outputPath}`);

  await createEpubLikeExample('100', bookName, author, chapters, outputPath);
  console.log(`\nCreated: 白鹿原.epub`);

  const timestamp = Date.now();

  const bookEntry = {
    "id": "100",
    "name": bookName,
    "author": author,
    "coverUrl": "app.media.book_image_1",
    "category": "文学",
    "rate": "9",
    "epubUrl": "books/白鹿原",
    "description": "《白鹿原》是一部渭河平原50年变迁的雄奇史诗，一轴中国农村斑斓多彩、触目惊心的长幅画卷。主人公白嘉轩六娶六丧，神秘的序曲预示着不祥。一个家族两代子孙，为争夺白鹿原的统治权争斗不已，上演了一幕幕惊心动魄的活剧。",
    "count": chapters.length.toString(),
    "popular": "10.5",
    "isFree": "0",
    "gender": "1",
    "groupName": "",
    "groupType": "0",
    "status": "1"
  };

  const data = {
    "/book/list": { "books": [bookEntry] },
    "/book/favourite": { "books": [bookEntry] },
    "/book/group": {
      "books": [{
        "bookName": bookName,
        "isSelect": false,
        "groupName": "",
        "singleBook": { ...bookEntry },
        "timestamp": timestamp,
        "isDownload": false,
        "downloading": false,
        "isPin": false
      }]
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
    "/book/recommend": { "books": [] },
    "/book/like": { "books": [] },
    "/book/child": { "books": [] }
  };

  fs.writeFileSync(DATA_JSON_PATH, JSON.stringify(data, null, 2));
  console.log('Updated data.json with 狼图腾 on bookshelf\n');
}

main().catch(console.error);
