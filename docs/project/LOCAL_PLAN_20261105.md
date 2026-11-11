# Lokálny plán ďalšieho vývoja — 5. november 2026

Tento dokument je podklad na spoločné rozhodnutie o ďalšom smerovaní.
Poradie vyjadruje závislosti a riziko; nejde o sľúbené termíny. Zmeny sú lokálne,
bez odoslania na GitHub. Stav nasadeného pilotu a stav novšieho zdrojového kódu
rozlišujeme: pilot má preferencie a 11 databázových tabuliek; pracovné priestory
sú pripravené v kóde a skúšobných prostrediach s 12 tabuľkami.

## Čo je vybavené a čo zostáva otvorené

| Oblasť | Stav | Dôkaz alebo ďalšia podmienka |
| --- | --- | --- |
| Denná záloha účtov a monitoringu | Hotové a reálne obnovené | Formát 2, rovnaká denná úloha; [obnova](RECOVERY_20261105.md) |
| Kontrola starších záloh pred čistením | Hotové | Obe databázy a súbory musia prejsť obnovou |
| Obnova do nových izolovaných zväzkov | Hotové | Všetkých 5 služieb zdravých, 11 tabuliek účtov, integrita monitoringu; skúšobné prostredie odstránené |
| Prenositeľný balík vrátane obrazov a CA | Hotové 6. novembra po súhlase používateľa | Šifrovaný balík obnovený, obrazy načítané, pôvodná CA a HTTPS dôvera overené v izolácii |
| Obnova na úplne novom počítači | Otvorené | Nové zväzky na existujúcom Docker prostredí nenahrádzajú druhý počítač |
| Bezpečnostný súpis všetkých závažností | Hotový súpis, pokračuje vecné preverovanie | [Triedenie nálezov](SECURITY_TRIAGE_20261105.md) |
| Podporovaný PyJWT v monitoringu | Nasadené 6. novembra | [Kvalifikácia a návrat](SECURITY_MONITOR_JWT_20261106.md), zachovaná história aj ostatné služby |
| Opraviteľné nálezy nižších závažností | Nasadené 11. novembra | [Go, zlib a XML parser](SECURITY_LOWER_20261111.md); presne štyri záznamy odstránené, žiadne ďalšie s dostupnou opravou; ostatné nálezy zostávajú na posúdenie |
| Úplnosť a obsah Gitu | Opätovne kontrolované 11. novembra | 729 sledovaných súborov; privátne dáta/výstupy mimo Gitu, kontrola aj v čistej kópii projektu, ochrana v CI |
| Pracovné priestory | Migrácia a návrat overené 11. novembra, pilot bez nasadenia | Pôvodné dáta/roly/oprávnenia zachované, nové zápisy prežili návrat a obnovu 12 tabuliek; [doklad](WORKSPACE_ROLLOUT_20261105.md) |
| Reštart Windows a automatické zotavenie | Zatiaľ nesplnené | Po reštarte 11. novembra bol Docker zastavený; ručné spustenie obnovilo služby, príčina zlyhania automatického štartu ostáva otvorená |

## Uzavretie aktuálneho kola opráv — 11. november

Lokálne opravy a ich skúšky sú uložené v commitoch; na GitHub sa neodosielali.
Aktualizované tri služby prešli živou kontrolou, zachovali dáta aj pôvodnú CA
a zálohy pred/po zmene sa reálne obnovili. Web a účty ostali na pôvodnej verzii,
bez nasadenia pracovných priestorov. Zlyhanie dennej zálohy už zachová posledný
dobrý bod a dohľad uchová diagnostiku posledného zlyhania aj po zotavení.

Ďalšie kroky pre spoločné rozhodnutie, v tomto poradí:

1. Overiť najbližšiu časovanú zálohu o 19:00. Dnešné vyvolanie cez plánovač
   skončilo úspešne, ale nedokazuje budúce automatické spustenie.
2. Potvrdiť, že obnovovacie heslo existuje na nezávislom mieste. Do chatu ho
   neposielať. Potom vybrať čistý počítač alebo virtuálny stroj na skúšku obnovy.
3. Dohodnúť krátke okno na reštart Windows a sledovať automatický návrat služieb.
   Tento bod ostáva otvorený; samotné ručné spustenie Dockeru ho neuzatvára.
4. Rozhodnúť o nasadení ručných pracovných priestorov. Migrácia a návrat aplikácie
   sú kvalifikované; ešte treba vybrať spoločné vydanie webu/účtov a overiť ho
   ako celok podľa etapy 5.
5. Následne pokračovať vecným posúdením zostávajúcich bezpečnostných nálezov
   a ďalším vývojom podľa etáp nižšie. Súpis 1 811 záznamov neznamená rovnaký
   počet zneužiteľných chýb ani automatické prijatie rizika.

Tieto body vyžadujú čas, zariadenie alebo rozhodnutie používateľa. Nejde o skryté
podmienky úspechu už overených lokálnych opráv.

## Etapa 1 — uzavrieť spoľahlivosť záloh

1. Sledovať najbližšie automatické denné spustenie: výsledok úlohy 0, čerstvý
   záznam úspechu, overenie monitoringu a stav zálohovacieho monitora UP.
   **11. november:** spustenie cez skutočný plánovač po obnove služieb prešlo;
   predchádzajúce denné spustenie zlyhalo. Najbližší časovaný beh ešte overiť.
2. Zopakovať skúšku odpojeného disku bez vytvorenia nového repozitára. Overiť,
   že upozornenie ukáže zlyhanie a posledný úspešný bod zostane dostupný.
3. Potvrdiť uloženie obnovovacieho hesla mimo počítača aj zálohovacieho disku.
   Heslo neposielať do chatu ani neukladať do Git histórie.
4. **Hotové 6. novembra:** používateľ schválil zálohu súkromného kľúča CA.
   Šifrovaný balík a obnovené HTTPS pod pôvodnou autoritou sú overené. Prípadnú
   budúcu rotáciu CA zahrnúť do novej kvalifikácie balíka.
5. Dohodnúť prijateľnú stratu dát a čas obnovy. Denná úloha pri prihlásenom
   používateľovi nezaručuje 24-hodinový bod obnovy pri vypnutom PC či odpojenom disku.
6. Rozhodnúť, či začať používať aj prílohy monitoringu. Denná záloha pokrýva jeho
   SQLite databázu; prípadné obrázky a iné súbory potrebujú vlastný rozsah a test.

**Podmienka uzavretia:** používateľ vie nájsť heslo, obnoviť obe databázy a pomenovať
reálnu stratu dát pri výpadku. Čistenie zostáva až za úspešným overením obnovy.

## Etapa 2 — preukázať obnovu mimo pôvodného prostredia

7. **Hotové 6. novembra:** overený obnovovací balík obsahuje presné obrazy,
   Git bundle, súkromnú konfiguráciu, obe databázy, potrebné pripojené súbory
   a pôvodnú CA. Na existujúcom Docker prostredí sa obnovil do nových zväzkov.
8. Overiť dostupnosť inštalačných nástrojov a ich verzií: Docker, Python, Git,
   Restic. Nezávisieť od existencie pôvodného adresára `build` alebo meniteľných tagov.
9. Použiť nový počítač alebo čistý virtuálny stroj. Obnoviť súbory do iného
   adresára, načítať obrazy z archívu a prepísať hostiteľské cesty.
10. Vytvoriť nové zväzky a roly, obnoviť dáta, spustiť služby a overiť prihlásenie,
    preferencie, výpočet, monitoring, históriu a dohodnutú dôveru HTTPS.
11. Zaznamenať čas, ručné zásahy a nedostupné závislosti. Návod opraviť podľa
    skutočného priebehu a zopakovať nejasné kroky.

**Podmienka uzavretia:** fungujúci systém bez čítania dát z pôvodných zväzkov,
adresárov či Docker cache. Súčasný izolovaný test je užitočný medzikrok.

## Etapa 3 — stabilita každodennej prevádzky

12. Dohodnúť jeden reálny reštart Windows. Sledovať štart Dockeru, pilotu,
    tunela, zdravie služieb a reakciu monitorov po prihlásení.
13. Pri ďalšom samovoľnom zastavení Dockeru zachytiť čas a systémové záznamy.
    Príčina predchádzajúcich zastavení nie je preukázaná; ďalšie opravné zásahy
    vybrať až podľa dôkazov.
    **11. november:** doplnený trvalý záznam posledného zlyhania dohľadu a
    jednoznačný neúspešný stav zálohy so zachovaním posledného dobrého bodu.
14. Zmerať rast histórie monitoringu, databáz a záloh počas bežnej prevádzky.
    Nastaviť limit voľného miesta a reakciu na jeho prekročenie.
15. Rozhodnúť o nezávislej kontrole dostupnosti mimo tohto PC. Lokálny monitor
    nevie ohlásiť úplný výpadok vlastného počítača.

**Podmienka uzavretia:** overený návrat po reštarte, merateľná čerstvosť záloh,
zachytenie ďalšieho výpadku a známy postup pri plnom disku.

## Etapa 4 — zvyšná bezpečnosť podľa skutočného použitia

16. **Pokrok 6. novembra:** monitoring používa oficiálny PyJWT 2.15.1 cez štandardný
    import, overený proti chybám verejných kľúčov aj skutočným OAuth/Apprise
    spotrebiteľom. Starší distribučný balík ostáva na disku a jeho záznamy skenera
    nezakrývame; prípadné priame použitie starého modulu treba hodnotiť samostatne.
17. Pre bránu zistiť dostupnosť problematických Go JOSE funkcií cez používané
    moduly. Pripraviť izolovaného kandidáta alebo úzky backport podľa výsledku.
18. Pre PostgreSQL samostatne posúdiť systémové SQLite. Neprítomnosť priameho
    prepojenia hlavného servera nie je dôkaz, že knižnicu nemôže použiť žiadny modul.
19. Prejsť MEDIUM/LOW/UNKNOWN po skupinách: libc, curl, util-linux, TLS/Kerberos,
    Python a ďalšie balíky. Každý záver viazať na konkrétnu chybu a spotrebiteľa.
20. Pri každom kandidátovi overiť novú databázu skenera, funkčné kontroly,
    kompatibilitu dát a návrat. Zmenu základného systému schváliť až po porovnaní
    textového správania; zamietnutý Alpine kandidát menil Unicode poradie a prevody.
21. Zaviesť pravidlo, kedy meniť súkromné opravy za opravu dodávateľa. Udržiavať
    pôvod zdrojov, kontrolné súčty, regresné testy a vlastníka každej odchýlky.

**Podmienka uzavretia jednej položky:** oprava s testom, konkrétny dôkaz
nepoužiteľnosti chyby pre daný balík alebo vedome prijaté a zaznamenané riziko.
Samotné „dodávateľ zatiaľ nemá opravu“ položku neuzatvára.

## Etapa 5 — nasadiť ručné pracovné priestory

22. Spoločne potvrdiť rozsah: pomenované uloženie vstupov a nastavení pre
    kalkulačku, grafy, rovnice a matice. Bez automatického spúšťania výpočtov pri otvorení.
23. Vybrať presnú revíziu, znovu zostaviť web aj účty s už overenými bezpečnostnými
    opravami a vykonať [nasadzovacie kontroly](WORKSPACE_ROLLOUT_20261105.md).
24. Nacvičiť migráciu `0004_workspace` na kópii aktuálnej databázy a návrat po
    vytvorení skúšobných pracovných priestorov. Nové používateľské dáta sa pri návrate
    nesmú potichu stratiť.
    **Hotové pre skúšobný pár 11. novembra:** migrácia, stará aplikácia na novej
    schéme, jej ďalší zápis, opätovné zapnutie kandidáta a obnova všetkých 12
    tabuliek prešli. Pred nasadením zostávajú úplné kontroly vybraného webu a účtov.
25. Až po kontrole podkladov nasadiť kandidáta, overiť dva účty, dve zariadenia,
    súbežné záložky, reštart a obmedzenia veľkosti. Potom vytvoriť novú zálohu a obnoviť ju.
26. Po období používania vyhodnotiť chyby, nejasné názvy a potrebu exportu.

**Podmienka uzavretia:** používateľ bezpečne uloží a otvorí rozpracované vstupy,
cudzie dáta sa nedajú čítať, konflikt ponechá lokálnu prácu a záloha obsahuje novú tabuľku.

## Etapa 6 — pohodlie práce až podľa spätnej väzby

27. Rozhodnúť o exporte/importovaní pracovných priestorov a verziovaní formátu.
    Import má mať rovnaké limity a validáciu ako uloženie cez rozhranie.
28. Ak sa ukáže potreba automatického ukladania, najprv dohodnúť interval,
    obnovu po strate spojenia a riešenie konfliktov medzi zariadeniami.
29. Zaviesť viditeľný stav posledného uloženia, zachovanie neodoslanej práce
    a ochranu pred oneskorenými odpoveďami po prepnutí účtu.
30. Overiť klávesnicu, kopírovanie a gestá grafov na fyzickom telefóne aj tablete.
    Simulovaný rozmer obrazovky túto skúšku nenahrádza.

## Etapa 7 — správa účtu a hranice súkromia

31. Dohodnúť profil, zmenu hesla a odhlásenie ďalších zariadení vrátane opätovného
    overenia pri citlivých úkonoch.
32. Navrhnúť export a vymazanie účtu aj pracovných priestorov. Jasne vysvetliť,
    dokedy môžu dáta existovať v starších zálohách a ako sa správa obnova.
33. Verejnú registráciu otvárať až po overení emailov, obnovy hesla, limitov
    zneužitia a doručovania správ. Najprv samostatne odsúhlasiť prevádzkový rozsah.

## Etapa 8 — uloženie výpočtového stavu a ďalšie matematické možnosti

34. Priradiť privátne výpočtové relácie prihláseným vlastníkom a testovať izoláciu.
35. Navrhnúť ohraničený prenos typovaných hodnôt a kontextu. Obnova sa nesmie
    skladať z opätovného vykonania výrazov alebo náhodných volaní.
36. Testovať presné hodnoty, neplatný vstup, nedostatok pamäte a nemennosť
    pôvodného stavu pri chybe; až potom prepájať uloženie relácie s pracovnými priestormi.
37. Ďalšie matematické funkcie a zrýchlenia vybrať podľa konkrétneho použitia,
    nezávislých správnostných referencií a opakovaných porovnateľných meraní.

## Rozhodnutia na najbližšie spoločné prejdenie

- Pôvodná CA už je po súhlase v overenej šifrovanej zálohe; dohodnúť jej ďalšiu rotáciu a ochranu obnovovacieho hesla.
- Cieľová strata dát a čas obnovy; umiestnenie ďalšej nezávislej kópie.
- Druhý počítač/virtuálny stroj a termín skúšky reštartu.
- Poradie bezpečnostnej údržby a nasadenia pracovných priestorov.
- Potreba exportu, automatického ukladania a správy zariadení.
- Kedy odoslať lokálne commity a overiť CI na presnej odoslanej revízii.

Po týchto rozhodnutiach upravíme poradie etáp. Neskoršie produktové etapy sú
návrhy na diskusiu, nie už schválený rozsah implementácie.
