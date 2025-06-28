const fs = require('fs');
const axios = require('axios');

const OWNER = 'SalatielSauer';
const REPO = 'SauerWebUI';

async function main() {
  const api = `https://api.github.com/repos/${OWNER}/${REPO}/releases/latest`;
  const headers = { };

  const { data } = await axios.get(api, { headers });
  const assets = data.assets;

  const zipAsset = assets.find(a => a.name.endsWith('.zip'));
  const exeAsset = assets.find(a => a.name.endsWith('_installer.exe'));

  if (!zipAsset || !exeAsset) {
    throw new Error('Could not find both ZIP and installer assets in the latest release.');
  }

  const zipMatch = zipAsset.name.match(/(\d{2}-\d{2}-\d{4})/);
  const exeMatch = exeAsset.name.match(/(\d{2}-\d{2}-\d{4})/);

  const zipDate = zipMatch ? zipMatch[1] : 'Unknown date';
  const exeDate = exeMatch ? exeMatch[1] : 'Unknown date';

  const zipLine = `[Download SauerWebUI (${zipDate}) ZIP](${zipAsset.browser_download_url})`;
  const exeLine = `[Download SauerWebUI (${exeDate}) installer](${exeAsset.browser_download_url})`;

  let readme = fs.readFileSync('README.md', 'utf-8');

  readme = readme.replace(
    /\[Download SauerWebUI \(\d{2}-\d{2}-\d{4}\) ZIP\]\(.*?\.zip\)/,
    zipLine
  ).replace(
    /\[Download SauerWebUI \(\d{2}-\d{2}-\d{4}\) installer\]\(.*?_installer\.exe\)/,
    exeLine
  );

  fs.writeFileSync('README.md', readme);
}

main().catch(err => {
  console.error(err);
  process.exit(1);
});
