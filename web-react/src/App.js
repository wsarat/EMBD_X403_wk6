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
          <option key={option.id} value={option.bssid}>
            {option.ssid} ({option.bssid})
          </option>
        ))}
      </select>
      {selectedOption && <p>You selected: {selectedOption}</p>}
    </div>
  );
};

const App = () => {
  const [data, setData] = useState([]);
  /*
  This line declares a state variable data with an initial value of an empty array, 
  and it also creates a state update function setData that can be used to update the value of the data state variable.
  */
  const [selectedOption, setSelectedOption] = useState('');

  useEffect(() => {
    const fetchData = async () => {
      try {
        const response = await fetch('http://esp32.local/api/scan');
        const data = await response.json();
        setData(data.ap_list);
      } catch (error) {
        console.error('Error fetching data:', error);
      }
    };

    fetchData();
  }, []);

  const handleSelectChange = (event) => {
    setSelectedOption(event.target.value);
  };

  return (
    <div>
      <h1>EMBD.X403 wk6</h1>
        <SelectList data={data} selectedOption={selectedOption} onChange={handleSelectChange} />
    </div>
  );
};

export default App;
