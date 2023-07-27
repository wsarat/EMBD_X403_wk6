import './App.css';
import React, { useState, useEffect } from 'react';

const SelectList = ({ data, selectedOption, onChange }) => {
  return (
    <div>
      <div>
        <label htmlFor="selectList">Access Point</label>
      </div>
      <select id="selectList" value={selectedOption} onChange={onChange}>
        <option value="">Select an option</option>
        {data.map((option) => (
          <option value={option.ssid}>
            {option.ssid} ({option.bssid})
          </option>
        ))}
      </select>
    </div>
  );
};


const App = () => {
  const [data, setData] = useState([]);
  /*
  This line declares a state variable data with an initial value of an empty array, 
  and it also creates a state update function setData that can be used to update the value of the data state variable.
  */
  const [ssid, setSsid] = useState('');
  const [password, setPassword] = useState('');

  useEffect(() => {
    const fetchData = async () => {
      try {
        const response = await fetch('/api/scan');
        const data = await response.json();
        setData(data.ap_list);
      } catch (error) {
        console.error('Error fetching data:', error);
      }
    };

    fetchData();
  }, []);

  const handleSelectChange = (event) => {
    setSsid(event.target.value)
  };

  const passwdChange = (event) => {
    setPassword(event.target.value)
  };  

  function setPasswd() {
    const jsonData = {};
    jsonData.ssid = ssid;
    jsonData.passwd = password;

    const postData = async (jsonData) => {
        try {
          const response = await fetch('/api/setAP', {  // Enter your IP address here
            method: 'POST', 
            mode: 'cors', 
            body: JSON.stringify(jsonData) // body data type must match "Content-Type" header
          });
          const data = await response.json();
          alert(JSON.stringify(data));

      } catch (error) {
        console.error('Error fetching data:', error);
      }

    };

    postData(jsonData);
  }

  return (
    <div>
      <h1>Wifi Setup</h1>
      <div>
        <SelectList data={data} selectedOption={ssid} onChange={handleSelectChange} />
      </div>
      <div>
        { ssid.length > 0 ? (
          <>
          <hr />
          <div>SSID: {ssid}</div>
          <div>Password: <input name="wifiPasswd" type="password" value={password} onChange={passwdChange}/></div>
          <div><button onClick={setPasswd}>Save</button></div>
          </>
        ):(
          <p>Please select Access Point.</p>
        )}
      </div>        
    </div>
  );
};

export default App;
