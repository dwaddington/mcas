import React from 'react';
import Header from 'gatsby-theme-carbon/src/components/Header';

const CustomHeader = (props) => (
  <Header {...props}>
    IBM Research&nbsp;<span>MCAS</span>
  </Header>
);

export default CustomHeader;
