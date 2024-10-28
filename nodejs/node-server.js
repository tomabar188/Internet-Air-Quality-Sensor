const server = require("express")(); //framework do aplikacji webowych
const mysql = require('mysql'); //import modułu do połączenia z bazą danych MySQL
var ServerTime=0;//zmienna przechowywująca czas, który służy do synchronizacji między serwerem a ESP

server.set('view engine', 'ejs') //silnik widoku - ejs (strony internetowe zapisane w tym formacie w celu przekazywania danych)


//var bodyParser = require('body-parser');
//server.use(bodyParser.json()); //obsługa zapytań POST w formacie JSON
var bp = require('body-parser');
server.use(bp.json()); //obsługa zapytań w formacie JSON

server.set('views', __dirname + '\\..\\views');

var db = mysql.createConnection({ //połączenie z bazą danych MySQL
  host: 'localhost',
  user: 'root',
  password: '123456789',
  database: 'espsensor'
  
})

db.connect(function(err) { //utworzenie połączenia z bazą danych MySQL
  if (err) throw err;
  console.log("Connected!");
  });


server.listen(8085, () => console.log("PORT 8085.")); //nasłuchiwanie na połączenie (od ESP32), w celu dalszej obsługi 



server.get('/check-connection', (req, res) => { //funkcja wykorzystywana do weryfikacji połączenia między serwerem a ESP
  res.send('ok');
    ServerTime= Date.now();//ustawienie znacznika - synchronizacja
    console.log(ServerTime);
  
  console.log('serv timne '+ServerTime);
});


server.post('/post', (req, res) => { //funkcja POST wykonywana na polecenie ESP
  
  let full_date = new Date(ServerTime+(req.body.time*1000)); 
  let cr_date = full_date.getFullYear()+'-'+(full_date.getMonth()+1)+'-'+full_date.getDate(); //zapis daty w podanym formacie
  let cr_time = full_date.getHours()+':'+full_date.getMinutes()+':'+full_date.getSeconds(); //zapis godziny w podanym formacie
  console.log(cr_date+' '+cr_time);
  
  let sql_post_pm10= 'INSERT INTO pm10 SET ?'; //zmienna przechowywująca polecenie POST dla wyników PM 10
  let sql_post_pm25='INSERT INTO pm25 SET ?'; //zmienna przechowywująca polecenie POST dla wyników PM 2.5

  db.query(sql_post_pm10,{Value_pm10: req.body.valuepm10, Date_pm10:cr_date, Time_pm10:cr_time}, (err, result) => { //POST wartości pm 10 do bazy danych
    if (err) throw err;
  });
  db.query(sql_post_pm25,{Value_pm25: req.body.valuepm25, Date_pm25:cr_date, Time_pm25:cr_time}, (err, result) => { //POST wartości pm 2.5 do bazy danych
    if (err) throw err;
  });
});




server.get('/all',function(req,res){ //zapytanie aktywne po wejściu na stronę internetową
   

  db.query('SELECT *,TIME_FORMAT(pm10.Time_pm10,\'%H:%i\') as Time FROM pm10 join pm25 on pm10.Id_pm10=pm25.Id_pm25 order by (pm10.Id_pm10);', (err, rows) =>{ //pobranie wszystkich rekordów z tabel dla wyników pm 10 i pm 2.5
      if(err) throw err;
      else{
          
          console.log(rows);
          res.render('index',{rows}) //wysłanie rekordów na stronę
      }
  })
})

server.get('/',function(req,res){ //zapytanie po wybraniu zakładki "ostatnie 24-godziny" na stronie internetowej
   

  db.query('SELECT *,TIME_FORMAT(pm10.Time_pm10,\'%H:%i\') as Time FROM pm10 join pm25 on pm10.Id_pm10=pm25.Id_pm25 order by (pm10.Id_pm10) DESC LIMIT 288;', (err, rows) =>{ //pobranie ostatnich 288 (rekordy dodawane są co 5 minut) rekordów z tabel dla wyników pm 10 i pm 2.5
      if(err) throw err;
      else{
          
          console.log(rows);
          res.render('index',{rows}) //wysłanie rekordów na stronę
      }
  })
})


server.get('/search', (req,res) => { //zapytanie po wybraniu poszczególnych parametrów wyszukiwania
 
  console.log(req.query.DateMax);
  
  let sql_query='SELECT *,TIME_FORMAT(pm10.Time_pm10,\'%H:%i\') as Time FROM pm10 join pm25 on pm10.Id_pm10=pm25.Id_pm25 where (Date_pm10 BETWEEN ? AND ? ) AND ((Time_pm10 BETWEEN ? AND ? )) and (Date_pm25 BETWEEN ? AND ? ) AND ((Time_pm25 BETWEEN ? AND ? ))'; //zapytanie spełniające kryteria wyszukiwania
  db.query(sql_query,[req.query.DateMin, req.query.DateMax, req.query.TimeMin, req.query.TimeMax,req.query.DateMin, req.query.DateMax, req.query.TimeMin, req.query.TimeMax ], (err, rows) =>{ //pobranie rekordów z bazy danych
    if(err) throw err;
    else{
        
      
        res.render('index',{rows})//wysłanie rekordów na stronę
    }
})
});

server.get('/datalist', (req,res) => {  //zapytanie po wybraniu zakładki "Lista pomiarów" na stronie internetowej
  
  console.log(req.query.DateMax);
  

  db.query('SELECT *,DATE_FORMAT(Date_pm10,"%m-%d-%Y") as DateFormat  FROM pm10 join pm25 on pm10.Id_pm10=pm25.Id_pm25 ', (err, rows) =>{ //pobranie rekordów wraz z datą w formacie łatwym do odczytania przez plik strony internetowej
    if(err) throw err;
    else{
        
       
        res.render('datalist',{rows}) //wysłanie rekordów na stronę
    }
})
});

server.get('/datalist-search', (req,res) => { //zapytanie po wybraniu zakładki "Lista pomiarów" i wybraniu kryteriów wyszukiwania
  
  console.log(req.query.DateMax);
  
  let sql_query = 'SELECT *,DATE_FORMAT(Date_pm10,"%m-%d-%Y") as DateFormat  FROM pm10 join pm25 on pm10.Id_pm10=pm25.Id_pm25 where (Date_pm10 BETWEEN ? AND ? ) AND ((Time_pm10 BETWEEN ? AND ? )) and (Date_pm25 BETWEEN ? AND ? ) AND ((Time_pm25 BETWEEN ? AND ? ))';//zapytanie spełniające kryteria wyszukiwania wraz z datą w formacie łatwym do odczytania przez plik strony internetowej
  db.query(sql_query,[req.query.DateMin, req.query.DateMax, req.query.TimeMin, req.query.TimeMax,req.query.DateMin, req.query.DateMax, req.query.TimeMin, req.query.TimeMax ], (err, rows) =>{//pobranie rekordów z bazy danych
    if(err) throw err;
    else{
        res.render('datalist',{rows})//wysłanie rekordów na stronę
    }
})
});



