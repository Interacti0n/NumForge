# NumForge: otvorené úlohy a roadmap

Dátum aktualizácie: 11. september 2026.

Zoznam zostávajúcej práce, nie hotových opráv. Aktuálne správanie opisujú
[API](docs/API.md), [návrh kalkulačky](docs/CALCULATOR_DESIGN.md)
a [testovanie](docs/TESTING.md). Nové funkcie sú neskoršia etapa.

## 1. Ďalšie vydanie

- Rozhodnúť, kedy vydať patch release; až potom zvoliť verziu, uzavrieť
  `Unreleased` v changelogu a pripraviť release poznámky a tag.

- Po pushnutí overiť nový browser job a rozšírené Clang fuzz kampane v CI;
  lokálne regresné a browser testy nenahrádzajú túto kontrolu.

## 2. Pred novými funkciami

- Zaviesť AST volania a register názvov, arity, domén a chýb. Pre viac argumentov
  plánujeme bodkočiarku: `gcd(12;18)`; zatiaľ nejde o platný vstup.
- Navrhnúť rozpoznávanie názvov ako `exp` bez rozbitia `πe`, `1e3` a veľkého
  `E`. Zachovať existujúce pravidlá čísel a implicitného násobenia.

Potom môžu nasledovať etapy, nie sľúbené čísla vydaní:

1. `abs`, `sign`, `min`, `max`, `gcd`, `lcm` a prípadne lokálne `ans`.
2. `isqrt`, `sqrt`, záporné celočíselné exponenty.
3. `exp`, `ln`, `log`, potom `sin`, `cos`, `tan`, rad/deg a redukcia argumentu.

Každá funkcia potrebuje doménové, presnostné a limitné testy aj SK/EN pomoc.

## 3. Odložené možnosti — nie podmienky najbližšieho vývoja

Tieto rozšírenia nie sú záväzným plánom implementácie; vrátime sa k nim podľa
potreby projektu.

- Podľa deklarovanej podpory neskôr doplniť macOS/ARM testovanie.

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
