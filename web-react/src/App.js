import './App.css';

import React, { useState, useEffect } from 'react';

const SelectList = ({ data, selectedOption, onChange }) => {
  return (
    <div>
      <label htmlFor="selectList">Select an option:</label>
      <select id="selectList" value={selectedOption} onChange={onChange}>
        <option value="">Select an option</option>
        {data.map((option) => (
          <option key={option.id} value={option.id}>
            {option.name}
          </option>
        ))}
      </select>
      {selectedOption && <p>You selected: {selectedOption}</p>}
    </div>
  );
};

const App = () => {
  const [data, setData] = useState([]);
  const [selectedOption, setSelectedOption] = useState('');

  useEffect(() => {
    const fetchData = async () => {
      try {
        const response = await fetch('https://api.example.com/data');
        const data = await response.json();
        setData(data);
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
      <h1>Sample App</h1>
      {data.length > 0 ? (
        <SelectList data={data} selectedOption={selectedOption} onChange={handleSelectChange} />
      ) : (
        <p>Loading data...</p>
      )}
    </div>
  );
};

export default App;
