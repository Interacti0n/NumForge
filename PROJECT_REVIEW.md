# NumForge: otvorené úlohy a roadmap

Dátum aktualizácie: 15. september 2026.

Zoznam zostávajúcej práce, nie hotových opráv. Aktuálne správanie opisujú
[API](docs/API.md), [návrh kalkulačky](docs/CALCULATOR_DESIGN.md)
a [testovanie](docs/TESTING.md). Poradie nižšie oddeľuje najbližší vývoj od
voliteľných rozšírení; rozpoznaný názov ešte neznamená hotový výpočet.

## 1. Overenie a prípadné vydanie

- Po ďalšom pushnutí overiť CI na presnom commite: všetky platformy,
  sanitizéry, fuzz kampane, browser testy a inštalačný consumer.
- Rozhodnúť, kedy vydať ďalší release; až potom zvoliť verziu, uzavrieť
  `Unreleased` v changelogu a pripraviť release poznámky a tag.

## 2. Implementácia nových funkcií

- Pre rozpoznávané, ale zatiaľ neimplementované funkcie postupne doplniť
  numerické algoritmy, domény a pravidlá presnosti; potom aktivovať tlačidlá.

Odporúčané poradie, nie sľúbené čísla vydaní:

1. `abs`, `sign`, `min`, `max`: presné desatinné operácie a porovnávanie.
2. `gcd`, `lcm`, `mod`, `isqrt`: celočíselná doména, nula a záporné vstupy;
   pred `mod` výslovne rozhodnúť, či ide o zvyšok so znamienkom delenca
   (ako BigInt API), alebo euklidovské modulo.
3. `sqrt`, `cbrt`, `root`: algoritmus, povolené stupne a záporné argumenty,
   pracovná presnosť, zaokrúhlenie a hraničné testy.
4. `exp`, `ln`, `log`: log(x) so základom 10, ln(x) so základom e,
   log(x;b) s vlastným základom; ošetriť domény a konvergenciu.
5. `sin`, `cos`, `tan`, `asin`, `acos`, jednoargumentový `atan`,
   `radians`, `degrees`: radiány, redukcia argumentu a presnosť pri póloch.

Pri každej etape: zapojiť implementáciu do registra bez prevodu na double,
pridať doménové, presnostné, limitné a alokačné testy, aktivovať príslušné
tlačidlá a aktualizovať SK/EN pomoc aj changelog. Najbližší konkrétny krok je
prvá štvorica `abs`, `sign`, `min`, `max`.

## 3. Odložené možnosti — nie podmienky najbližšieho vývoja

Tieto rozšírenia nie sú záväzným plánom implementácie; vrátime sa k nim podľa
potreby projektu.

- Podľa deklarovanej podpory neskôr doplniť macOS/ARM testovanie.
- Záporné celočíselné exponenty a lokálne `ans` riešiť ako samostatné rozšírenia.

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
