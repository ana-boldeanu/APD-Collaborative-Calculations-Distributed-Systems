// Boldeanu Ana-Maria
// APD - Tema 3 - Calcule colaborative in sisteme distribuite

============================== Descrierea solutiei ============================

    1. Stabilirea topologiei

    Inca de la inceput, am presupus ca legatura [0-1] lipseste, pentru a rezolva
cerinta 3 in mod automat.
    
    Pentru topologie, tin un vector de parinti int parents[num_tasks] in care
fiecare coordonator isi va nota workerii asignati.
    Practic, valoarea parents[i] semnifica cine este coordonatorul procesului i.

    Se va parcurge inelul in ordinea 0 -> 3 -> 2 -> 1 -> 2 -> 3 -> 0, dupa 
logica de mai jos:
    Primul coordonator initializeaza vectorul cu valorea -1 pentru toate 
intrarile, apoi citeste din fisierul de input care sunt workerii lui si ii 
actualizeaza in parents. Apoi, trimite topologia partiala catre coordonatorul 3.
Acesta isi adauga workerii si trimite mai departe, iar la final coordonatorul 1
va avea topologia completa.
    Acum, coordonatorul 1 trimite topologia completa catre toti workerii sai si
inapoi catre coordonatorul 2, care o trimite la randul sau catre workerii si 
catre coordonatorul 3, care repeta procesul spre 0.

    Workerii stau idle pana cand primesc topologia de la ANY_SOURCE (deoarece 
initial nu isi stiu coordonatorul). Cand au topologia, isi extrag si rangul 
clusterului de acolo.

    Pentru bonus, logica este aceeasi, insa inelul se parcurge in ordinea
0 -> 3 -> 2 -> 3 -> 0, iar coordonatorul 1 isi calculeaza propria sa 
topologie (care contine doar workerii sai).
    Mai exact, pe codul deja existent, am conditionat mesajele catre si 
de la coordonatorul 1, astfel incat ele sa nu se trimita daca acesta 
este deconectat (i.e., daca error_type == 2).

=============================================================================

    2. Realizarea calculelor

    Coordonatorul 0 construieste vectorul de valori initiale si, pe baza 
topologiei, calculeaza numarul de workeri disponibili. Pentru bonus, 
la sfarsitul calcului topologiei principale, parents[i] va avea valorile
-1 pentru workerii care apartin clusterului 1 (deoarece acesta nu a 
participat la calcul). Astfel, se considera disponibili doar workerii 
al caror parent este printre 0, 2 si 3.

    Coordonatorul 0 calculeaza marimea unui chunk, adica portiunea care
va fi trimisa fiecarui worker. Am impartit dimensiunea vectorului la 
numarul workerilor disponibili, urmand ca ultimul worker sa acopere 
chunkul lui + ce a mai ramas din vector, in cazul in care divizia 
nu a avut restul 0.

    Inelul se parcurge in ordinea 0 -> 3 -> 2 (-> 1 -> 2) -> 3 -> 0.

    Fiecare coordonator va primi vectorul neprocesat, dimensiunea 
unui chunk, si indexul chunkului de la care incepe procesarea pentru 
clusterul sau.
    Avand aceste informatii, coordonatorul calculeaza indicii de inceput 
si sfarsit pentru fiecare worker si ii trimite acestora, impreuna cu 
vectorul neprocesat.
    Apoi, avand valoarea ultimului index de chunk, fiecare coordonator 
poate trimite aceasta valoare catre urmatorul.
    Workerii isi calculeaza intervalul si trimit vectorul modificat catre 
parinte, impreuna cu indicii de start si sfarsit asignati.

    Ultimul coordonator din lant (fie el 1 sau 2) va incepe asamblarea
rezultatelor finale. Pentru aceasta, am folosit un vector separat, int 
results[array_size].
    Ultimul coordonator primeste rezultatele partiale de la workeri si
le scrie in vectorul de rezulte finale, pe care il trimite mai departe 
catre coordonatorul precedent (fie el 2 sau 3).
    Procesul se repeta pana cand toti coordonatorii si-au adaugat
rezultatele workerilor in acest vector, mai precis la coordonatorul 0,
care si afiseaza rezultatul final.

    Pentru bonus, procesele care apartin clusterului 1 nu participa la 
calculul vectorului. Practic, in cod, a fost de ajuns sa adaug cate un 
break; la coordonator si la workeri.

=========================================================================
