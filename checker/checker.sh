#!/bin/bash

topo_tests=0
comp_tests=0
invalid_comm=0
disconnect=0
bonus=0

# afiseaza scorul final
function show_score {
	local invalid=0
	local total=0

	if [ $invalid_comm == 1 ]
	then
		invalid=100
		total=0
	else
		invalid=0
		total=$((topo_tests+comp_tests+disconnect+bonus))
	fi

	echo ""
	echo "Scor topologie: $topo_tests/30"
	echo "Scor corectitudine: $comp_tests/30"
	echo "Scor deconectare: $disconnect/20"
	echo "Depunctare mesaje: $invalid/100"
	echo "Bonus: $bonus/15"
	echo "Total: $total/95"
}

# se verifica topologia (parametru: numarul de procese)
function check_topology {
	local topology=`cat topology.txt`
	local procs=$(($1-1))
	local ok=0

	for i in `seq 0 $procs`
	do
		cat out.txt | grep -q -i "$i -> $topology"
		ok=$((ok+$?))
	done

	if [ $ok == 0 ]
	then
		topo_tests=$((topo_tests+10))
	else
		echo "E: Topologia nu este afisata corect de toate procesele"
	fi
}

# se verifica daca rezultatul calculelor este corect
function check_computation {
	local output=`cat output.txt`
	local ok=0

	cat out.txt | grep -q -i "$output"
	
	if [ $? == 0 ]
	then
		comp_tests=$((comp_tests+10))
	else
		echo "E: Rezultatul final nu este corect"
		echo "Se astepta:"
		echo $output
		echo "S-a gasit:"
		cat out.txt | grep "Rezultat"
	fi
}

# se verifica daca sunt mesaje incorecte (parametru out: rezultate invalide)
function check_messages {
	local __resultvar=$1
	local messages=`cat out.txt | grep "M("`
	local count=0
	local good=0

	for m in $messages
	do
		count=$((count+1))

		if grep -Fxq "$m" allowed.txt
		then
		    good=$((good+1))
		else
		    echo "E: S-a detectat comunicatie intre doua procese neconectate: $m"
		fi
	done

	if [ $count == $good ]
	then
		eval $__resultvar=1
	else
		eval $__resultvar=0
	fi
}

# se verifica daca a treia si a patra cerinte sunt implementate corect (parametru: cerinta verificata)
function check_disconnect_and_partition {
	local procs=`sed '1q;d' inputs.txt`
	local N=`sed '2q;d' inputs.txt`
	local output=`cat output.txt`
	local ok=0
	local count=0
	local good=0

	local dc=$(($1-2))

	mpirun --oversubscribe -np $procs ./tema3 $N $dc &> out.txt

	# verificare topologie
	procs=$(($procs-1))

	if [ $1 == 3 ]
	then
		local topology=`cat topology.txt`

		for i in `seq 0 $procs`
		do
			cat out.txt | grep -q -i "$i -> $topology"
			ok=$((ok+$?))
		done
	else
		local topology_large=`cat topology_large.txt`
		local topology_small=`cat topology_small.txt`

		for i in `seq 0 $procs`
		do
			if [ $i == 1 ] || [ $i == 5 ] || [ $i == 9 ] 
			then
				cat out.txt | grep -q -i "$i -> $topology_small"
				ok=$((ok+$?))
			else
				cat out.txt | grep -q -i "$i -> $topology_large"
				ok=$((ok+$?))
			fi
		done
	fi

	if [ $ok != 0 ]
	then
		echo "E: Topologia nu este afisata corect de toate procesele"
		return
	fi

	# verificare rezultat calcul
	ok=0
	cat out.txt | grep -q -i "$output"

	if [ $? != 0 ]
	then
		echo "E: Rezultatul final nu este corect"
		echo "Se astepta:"
		echo $output
		echo "S-a gasit:"
		cat out.txt | grep "Rezultat"
		return
	fi

	#verificare mesaje intre procese
	local messages=`cat out.txt | grep "M("`

	for m in $messages
	do
		count=$((count+1))

		if grep -Fxq "$m" allowed.txt
		then
		    good=$((good+1))
		else
		    echo "E: S-a detectat comunicatie intre doua procese neconectate: $m"
		    return
		fi
	done

	if [ $1 == 3 ]
	then
		disconnect=20
	else
		bonus=15
	fi
}

# printeaza informatii despre rulare
echo "VMCHECKER_TRACE_CLEANUP"
date

export OMPI_ALLOW_RUN_AS_ROOT=1
export OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1

# se compileaza tema
cd ../src
make clean &> /dev/null
make build &> build.txt

if [ ! -f tema3 ]
then
    echo "E: Nu s-a putut compila tema"
    cat build.txt
    show_score
    rm -rf build.txt
    exit
fi

rm -rf build.txt

mv tema3 ../checker
cd ../checker

# se ruleaza 4 teste
for i in `seq 1 3`
do
	echo ""
	echo "Se ruleaza testul $i..."

	cp ./tests/test$i/*.txt .

	procs=`sed '1q;d' inputs.txt`
	N=`sed '2q;d' inputs.txt`

	mpirun --oversubscribe -np $procs ./tema3 $N 0 &> out.txt

	# verificare topologie
	check_topology $procs

	# verificare rezultat calcul
	check_computation

	#verificare mesaje intre procese
	check_messages invalid
	if [ $invalid == 0 ]
	then
		invalid_comm=1
	fi

	rm -rf *.txt
done

# se ruleaza testul de deconectare
echo ""
echo "Se ruleaza testul de deconectare..."
cp ./tests/test_disconnect/*.txt .
check_disconnect_and_partition 3
rm -rf *.txt

# se ruleaza testul bonus
echo ""
echo "Se ruleaza testul de bonus..."
cp ./tests/test_partition/*.txt .
check_disconnect_and_partition 4
rm -rf *.txt

make clean &> /dev/null

show_score