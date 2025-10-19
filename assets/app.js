(function(){
  const yearEl = document.getElementById('year');
  if (yearEl) yearEl.textContent = new Date().getFullYear();

  const versionEl = document.getElementById('version');
  const winLink = document.getElementById('winLink');
  const debLink = document.getElementById('debLink');

  const owner = 'ConsoleMangena';
  const repo = 'sitesurveyor';

  fetch(`https://api.github.com/repos/${owner}/${repo}/releases/latest`, {headers:{'Accept':'application/vnd.github+json'}})
    .then(r => r.ok ? r.json() : Promise.reject(r))
    .then(data => {
      if (versionEl) versionEl.textContent = `${data.name || data.tag_name} · ${new Date(data.published_at).toLocaleDateString()}`;
      const assets = data.assets || [];
      const win = assets.find(a => /Windows\.zip$/i.test(a.name));
      const deb = assets.find(a => /Debian-amd64\.deb$/i.test(a.name));
      if (win && win.browser_download_url && winLink) winLink.href = win.browser_download_url;
      if (deb && deb.browser_download_url && debLink) debLink.href = deb.browser_download_url;
    })
    .catch(() => { if (versionEl) versionEl.textContent = 'latest'; });
})();