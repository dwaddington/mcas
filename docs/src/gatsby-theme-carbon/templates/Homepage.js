import React from 'react';
import { HomepageBanner, HomepageCallout } from 'gatsby-theme-carbon';
import HomepageTemplate from 'gatsby-theme-carbon/src/templates/Homepage';
//import { calloutLink } from './Homepage.module.scss';
//import Carbon from '../../images/carbon.jpg';
import BlueHwScape from '../../images/hwscape.png';

const FirstLeftText = () => (
//        <a href="../images/1x/MCAS_logo_wb.png"></a>
//       pre-shadowed homepage template
//     </a
    <p></p>
);

const FirstRightText = () => (

  <main>

    <p><strong>Overview</strong>
    <br />
    Persistent Memory is redefining how we think about storage systems.  By
    keeping durable data close to the CPU, efficient operations can be performed
    in-place without data movement.  MCAS is an advanced new storage system that evolves
    the key-value paradigm to unleash this potential.
    </p>
    <br></br>
  </main>
);


//      avoid read/write amplification
//      that arises in traditional block-centric storage systems.

//      Coupled with high-performance networking and zero-copy data movement, Persistent Memory
//      can provide unprecedented performance.

// const FirstRightText = () => (
//   <p>
//     This is a callout component. You can edit the contents by updating the{' '}
//     <a href="https://github.com/carbon-design-system/gatsby-theme-carbon/blob/5fe12de31bb19fbfa2cab7c69cd942f55aa06f79/packages/example/src/gatsby-theme-carbon/templates/Homepage.js">
//       pre-shadowed homepage template
//     </a>
//     . You can also provide <code>color</code> and <code>backgroundColor</code>{' '}
//     props to suit your theme.
//     <a
//       className={calloutLink}
//       href="https://github.com/carbon-design-system/gatsby-theme-carbon/blob/master/packages/example/src/gatsby-theme-carbon/templates/Homepage.js"
//     >
//       Homepage source →
//     </a>
//   </p>
// );


const SecondLeftText = () => (
<p>
        "As soon as consistency models become slightly difficult to understand for application developers, we see that they are ignored even if performance could be improved. The bottom line is that if the semantics of a consistency model are not intuitively clear, application developers will have a hard time building correct applications" - <strong>Andrew Tanenbaum, Maarten Steen</strong>
        </p>

);

const SecondRightText = () => <p></p>;
//   <p>
//     You can also not use these components at all by not providing the callout
//     props to the template or writing your own template.
//     <a
//       className={calloutLink}
//       href="https://github.com/carbon-design-system/gatsby-theme-carbon/blob/master/packages/example/src/gatsby-theme-carbon/templates/Homepage.js"
//     >
//       Homepage source →
//     </a>
//   </p>
// );

const BannerText = () => <h1>Memory-Centric Active Storage</h1>;

const customProps = {
  Banner: <HomepageBanner renderText={BannerText} image={BlueHwScape} />,
  FirstCallout: (
    <HomepageCallout
      backgroundColor="#030303"
      color="white"
      leftText={FirstLeftText}
      rightText={FirstRightText}
    />
  ),
  SecondCallout: (
    <HomepageCallout
      leftText={SecondLeftText}
      rightText={SecondRightText}
      color="white"
      backgroundColor="#061f80"
    />
  ),
};

// spreading the original props gives us props.children (mdx content)
function ShadowedHomepage(props) {
  return <HomepageTemplate {...props} {...customProps} />;
}


export default ShadowedHomepage;
