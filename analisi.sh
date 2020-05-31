#!/bin/bash
pid=`cat ./var/run/sm.pid`;
while ps -p $pid > /dev/null 2>&1
do 
    sleep 0.5;
done
logfile=`cat ./var/run/log_filename.txt`
casse=0;
echo "--- STATISTICHE SUPERMERCATO ---";
echo "CLIENTI";
printf "| %6s | %6s | %8s | %8s | %6s |\n" "id" "# prod" "t tot" "t coda" "# code"
while read -r line
do
    IFS=';' token=( $line );
    res=$(echo $line | grep -E 'SUPERMERCATO;*' | wc -l);
    if [ $res -eq 1 ]; then
        echo "NUMERO TOTALE DI CLIENTI = ${token[1]}";
        echo "NUMERO TOTALE DI PRODOTTI PROCESSATI = ${token[2]}";
        continue;
    fi
    res=$(echo $line | grep -E 'CLIENTE;*' | wc -l);
    if [ $res -eq 1 ]; then
        t_tot=$(echo "scale=3; ${token[2]} / 1" | bc);
        t_coda=$(echo "scale=3; ${token[3]} / 1" | bc);
        printf "| %6d | %6d | %8s | %8s | %6d |\n" ${token[1]} ${token[6]} $t_tot $t_coda $(( ${token[5]} + 1 ));
        continue;
    fi
    res=$(echo $line | grep -E 'CASSA;*' | wc -l);
    if [ $res -eq 1 ]; then
        if [ $casse -eq 0 ]; then
            echo "CASSE";
            printf "| %6s | %6s | %9s | %8s | %7s | %10s |\n" "id" "# prod" "# clienti" "t aper" "t medio" "# chiusure";
            casse=1;
        fi
        idcassa=${token[1]};
        num_clienti=${token[2]};
        num_prodotti=${token[3]};
        num_chiusure=${token[4]};
        t_apertura=0;
        continue;
    fi
    res=$(echo $line | grep "SERVIZIO" | wc -l);
    if [ $res -eq 1 ]; then
        t_servizio=0;
        continue;
    fi
    res=$(echo $line | grep "[0-9]\." | wc -l);
    if [ $res -eq 1 ]; then
        t_apertura=$(echo "scale=3; ($t_apertura + ${token[0]}) / 1" | bc);
        continue;
    fi
    res=$(echo $line | grep "FINE" | wc -l);
    if [ $res -eq 1 ]; then
        if [ $num_clienti -ne 0 ]; then
            t_medio=$(echo "scale=3; $t_servizio / $num_clienti / 1000" | bc);
        else
            t_medio=0;
        fi
        printf "| %6d | %6d | %9d | %8s | %7s | %10d |\n" $idcassa $num_prodotti $num_clienti $t_apertura $t_medio $num_chiusure;
        continue;
    fi
    res=$(echo $line | grep "[(0-9)]*" | wc -l);
    if [ $res -eq 1 ]; then
        t_servizio=$(( $t_servizio + ${token[0]} ));
        continue;
    fi
done < $logfile
exit;
