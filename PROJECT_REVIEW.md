# NumForge: otvorené úlohy a roadmap

Dátum aktualizácie: 11. september 2026.

Zoznam zostávajúcej práce, nie hotových opráv. Aktuálne správanie opisujú
[API](docs/API.md), [návrh kalkulačky](docs/CALCULATOR_DESIGN.md)
a [testovanie](docs/TESTING.md). Nové funkcie sú neskoršia etapa.

## 1. Uzavretie aktuálneho balíka

- Po commite a pushnutí overiť GitHub Actions: Linux so sanitizérmi, Linux
  32-bit, Windows, nainštalovaný balík a nové Clang/libFuzzer/coverage joby.
  Lokálne testy nenahrádzajú CI; Clang/libFuzzer profil čaká na prvý vzdialený beh.
- Až podľa výsledkov rozhodnúť o patch release a jeho poznámkach.

## 2. Testy a výkon: ďalší stupeň

- Doplniť fuzz harness pre kalkulačný formatter a HTTP framing. Pre formatter
  najprv určiť očakávania pri extrémnom vedeckom výstupe; pre HTTP oddeliť
  spracovanie bajtov od socketov, aby sa dalo efektívne fuzzovať bez servera.
- Doplniť reálny browser test vrátane navigácie SK/EN počas chýb, ovládania
  klávesnice a clipboardu; vybrať a uzamknúť testovaciu browser závislosť.
- Podľa deklarovanej podpory neskôr doplniť macOS/ARM.

## 3. Pred novými funkciami

- Zaviesť AST volania a register názvov, arity, domén a chýb. Pre viac argumentov
  plánujeme bodkočiarku: `gcd(12;18)`; zatiaľ nejde o platný vstup.
- Navrhnúť rozpoznávanie názvov ako `exp` bez rozbitia `πe`, `1e3` a veľkého
  `E`. Zachovať existujúce pravidlá čísel a implicitného násobenia.
- Rozhodnúť o spätnej načítateľnosti výstupu s exponentom mimo rozsahu
  vstupného parsera pri extrémnych mierkach.

Potom môžu nasledovať etapy, nie sľúbené čísla vydaní:

1. `abs`, `sign`, `min`, `max`, `gcd`, `lcm` a prípadne lokálne `ans`.
2. `isqrt`, `sqrt`, záporné celočíselné exponenty.
3. `exp`, `ln`, `log`, potom `sin`, `cos`, `tan`, rad/deg a redukcia argumentu.

Každá funkcia potrebuje doménové, presnostné a limitné testy aj SK/EN pomoc.

## 4. Odložené možnosti — nie podmienky najbližšieho vývoja

Tieto rozšírenia nie sú záväzným plánom implementácie; vrátime sa k nim podľa
potreby projektu.

### Presnosť

- Samostatne nastaviteľná pracovná presnosť a informácie `rounded`/`inexact`.
- Adaptívne prepočítavanie alebo intervalová kontrola chyby, ak budeme chcieť
  garantovať správne zaokrúhlený výsledok celého výrazu.
- Dopočítavanie konštánt nad 200 uložených desatinných miest.

### Architektúra a optimalizácie

Lokálna aplikácia zostáva sekvenčný loopback server s kooperatívnym rušením.
Konfigurovateľné rozpočty alebo worker proces pre tvrdé zrušenie sú samostatné
rozšírenia. Verejné nasadenie potrebuje frontu/pool, súbežné pamäťové limity,
ukončovanie procesov a bezpečnostnú politiku.

Po meraniach zvážiť prefixovú konverziu s ochrannou číslicou a sticky bitom,
všeobecné rýchle delenie malým deliteľom/mocninou desiatich, menej textových
prevodov, normalizované dlhé delenie a Karatsuba násobenie.

Dlhodobo: premenné, BigRational/BigComplex, WASM worker, bindings, jednotky,
grafy či symbolická algebra. Databáza až pre účty alebo synchronizáciu.

Po dokončení položku odstrániť a správanie zapísať do príslušnej dokumentácie
a changelogu, aby review zostával plánom zostávajúcej práce.
