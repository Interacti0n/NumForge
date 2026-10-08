// Resolve repository paths independently of the caller's working directory.
process.chdir(require('node:path').resolve(__dirname, '..'));
const fs=require('fs'),path=require('path');
const {execFileSync}=require('child_process');
const files=new Set(execFileSync('git',['ls-files'],{encoding:'utf8'}).trim().split('\n').filter(f=>fs.existsSync(f)&&f.endsWith('.md')));
function walk(dir){for(const e of fs.readdirSync(dir,{withFileTypes:true})){const f=dir+'/'+e.name;if(e.isDirectory())walk(f);else if(f.endsWith('.md'))files.add(f);}}
walk('docs');files.add('CHANGELOG_SHORT.md');
let count=0;const errors=[];
const slug=s=>s.replace(/\[([^\]]+)\]\([^)]*\)/g,'$1').replace(/<[^>]+>/g,'').toLowerCase().replace(/[^\p{L}\p{N}_\-\s]/gu,'').replace(/ /g,'-');
for(const f of files){const text=fs.readFileSync(f,'utf8');for(const match of text.matchAll(/!?\[[^\]\n]*\]\(([^\s)]+)\)/g)){const target=match[1];if(/^(https?:|mailto:|app:|codex:)/.test(target))continue;const [file,anchor]=target.split('#');const resolved=file?path.resolve(path.dirname(f),decodeURIComponent(file)):path.resolve(f);count++;if(!fs.existsSync(resolved)){errors.push(f+': missing '+target);continue;}if(anchor&&resolved.endsWith('.md')){const slugs=new Set();const repetitions={};for(const h of fs.readFileSync(resolved,'utf8').matchAll(/^#{1,6}\s+(.+)$/gm)){let id=slug(h[1].trim());const n=repetitions[id]||0;repetitions[id]=n+1;slugs.add(id+(n?'-'+n:''));}if(!slugs.has(anchor))errors.push(f+': missing anchor '+target);}}}
if(errors.length){console.error(errors.join('\n'));process.exitCode=1;}else console.log(`${files.size} Markdown files, ${count} local links/anchors verified.`);
