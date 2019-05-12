#/bin/bash
cd /home/reario/cancello
grep -q "EXITING" cancello.log && echo "cancello exited" | mutt -s "Cancello daemon terminato" vittorio.giannini@windtre.it 
