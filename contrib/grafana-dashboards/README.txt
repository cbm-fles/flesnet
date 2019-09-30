Grafana Dashboards 

Importieren über das UI:

In dem Reiter neues Dashboard anlegen ist der auch der IMPORT.
Die drei vorhandenen files können dort hochgeladen werden. 
Beim Import sollte direkt die Datenbank ausgewählt werden können,
sofern diese als Datenquelle festgelegt ist.

-------------------------------------------------------------------------------

Importieren über die .init

Bei den neueren Versionen von Grafana sollte es möglich sein, Dashboards von
der Festplatte aus zu laden. 

Dazu müssen 'dashboard'.yaml files erstellt werden, die auf die Dashboards
verweisen.
Genaueres kann der Website unter:

https://grafana.com/docs/administration/provisioning/#dashboards

entnommen werden. 

-------------------------------------------------------------------------------

Die panel arbeiten mit 'group_by(interval)'. interval sollte nicht kleiner
sein, als die Zeit zwischen HTTP-Anfragen.
Bsp: Wird alle 10s eine HTTP-Anfrage von den Host versandt, sollte
interval >= 10s.
Ansonsten werden unzusammenhängende Punkte und keine Graphen angezeigt. 

Panel mit mehreren Datenbankabfragen (Bsp fifo fill oder viele aus Flib_Status)
lassen sich über eine Variable '__interval' steuern. Diese befindet sich 
unter allen Einstellungen zu den Datenbankabfragen.

Leider ist diese Variable als Grafana globale Variable geschützt und kann
zum jetztigen Zeitpunkt (30.09.2019) nicht als Auswahlliste gehandhabt werden.

-------------------------------------------------------------------------------
